#include "connection_pool.h"
#include "logging.h"
#include "runtime_context.h"
#include "imu_time_sync.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static connection_pool_type* pool = NULL;

static void ensure_capacity() {
    if (pool->count >= pool->capacity) {
        int newcap = pool->capacity == 0 ? 2 : pool->capacity + 1;
        pool->list = (connection_t**)realloc(pool->list, newcap * sizeof(connection_t*));
        pool->capacity = newcap;
    }
}

void connection_pool_init() {
    pool = (connection_pool_type*)calloc(1, sizeof(*pool));
    pthread_mutex_init(&pool->mutex, NULL);
    pool->primary_index = -1;
    pool->supplemental_index = -1;
    pool->rate_est_primary = imu_rate_estimator_create(256);
    pool->rate_est_supp = imu_rate_estimator_create(256);
    pool->time_sync = NULL;
    pool->time_sync_initialized = false;
    pool->last_offset_s = 0.0f;
    pool->last_confidence = 0.0f;
}

static int pick_primary_index() {
    for (int i = 0; i < pool->count; ++i) {
        if (!pool->list[i]->device->can_be_supplemental) return i;
    }
    return pool->count > 0 ? 0 : -1;
}

static int pick_supplemental_index() {
    for (int i = 0; i < pool->count; ++i) {
        if (i != pool->primary_index && pool->list[i]->supplemental) return i;
    }
    return -1;
}

static connection_t* primary() {
    if (pool->primary_index < 0 || pool->primary_index >= pool->count) return NULL;
    return pool->list[pool->primary_index];
}

static connection_t* supplemental() {
    if (pool->supplemental_index < 0 || pool->supplemental_index >= pool->count) return NULL;
    return pool->list[pool->supplemental_index];
}

static void* block_thread_func(void* arg) {
    connection_t* c = (connection_t*)arg;
    if (config()->debug_connections) log_debug("block_thread_func %s\n", c->driver->id);
    c->driver->block_on_device_func();
    c->thread_running = false;
    return NULL;
}

static void connection_pool_start_connection_thread(connection_t* c) {
    if (c && !c->thread_running) {
        c->active = true;
        c->thread_running = true;
        pthread_create(&c->thread, NULL, block_thread_func, c);
    }
}

void connection_pool_handle_device_added(const device_driver_type* driver, device_properties_type* device) {
    if (config()->debug_connections) log_debug("connection_pool_handle_device_added for driver %s\n", driver->id);

    pthread_mutex_lock(&pool->mutex);

    ensure_capacity(pool);
    connection_t* c = (connection_t*)calloc(1, sizeof(*c));
    c->driver = driver;
    c->device = device; // take ownership
    c->supplemental = device->can_be_supplemental;
    c->active = false;
    pool->list[pool->count++] = c;

    // If no primary selected yet, try to pick one
    connection_t* p = primary();
    bool blocked_on_active = false;
    if (!p) {
        pool->primary_index = pick_primary_index(pool);
        if (config()->debug_connections) log_debug("connection_pool_handle_device_added picked primary %d\n", pool->primary_index);
    } else {
        blocked_on_active = p->active && p->thread_running;
    }

    // If possible, choose a supplemental distinct from primary
    if (pool->supplemental_index == -1) {
        pool->supplemental_index = pick_supplemental_index(pool);
        if (config()->debug_connections) log_debug("connection_pool_handle_device_added picked supplemental %d\n", pool->supplemental_index);

        connection_t* s = supplemental();
        if (blocked_on_active && s) {
            connection_pool_start_connection_thread(s);
        }
    }

    pthread_mutex_unlock(&pool->mutex);
}

bool connection_pool_is_connected() {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary();
    bool connected = p && p->driver->is_connected_func();
    pthread_mutex_unlock(&pool->mutex);
    return connected;
}

bool connection_pool_device_is_sbs_mode() {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary();
    bool enabled = p && p->driver->device_is_sbs_mode_func();
    pthread_mutex_unlock(&pool->mutex);
    return enabled;
}

bool connection_pool_device_set_sbs_mode(bool enabled) {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary();
    bool ok = p && p->driver->device_set_sbs_mode_func(enabled);
    pthread_mutex_unlock(&pool->mutex);
    return ok;
}

void connection_pool_disconnect_all(bool soft) {
    if (config()->debug_connections) log_debug("connection_pool_disconnect_all %s\n", soft ? "soft" : "hard");
    pthread_mutex_lock(&pool->mutex);
    for (int i = 0; i < pool->count; ++i) {
        connection_t* c = pool->list[i];
        if (c) c->driver->disconnect_func(soft);
        c->active = false;
    }
    // Reset time sync state
    pool->time_sync_initialized = false;
    if (pool->time_sync) { imu_time_sync_destroy(pool->time_sync); pool->time_sync = NULL; }
    if (pool->rate_est_primary) { imu_rate_estimator_destroy(pool->rate_est_primary); pool->rate_est_primary = imu_rate_estimator_create(256); }
    if (pool->rate_est_supp) { imu_rate_estimator_destroy(pool->rate_est_supp); pool->rate_est_supp = imu_rate_estimator_create(256); }
    pthread_mutex_unlock(&pool->mutex);
}

bool connection_pool_connect_active() {
    if (config()->debug_connections) log_debug("connection_pool_connect_active\n");
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary();
    connection_t* s = supplemental();
    pthread_mutex_unlock(&pool->mutex);

    bool pr_ok = false;
    if (p) pr_ok = p->driver->device_connect_func();
    if (s) (void)s->driver->device_connect_func();
    return pr_ok;
}

void connection_pool_block_on_active() {
    if (config()->debug_connections) log_debug("connection_pool_block_on_active\n");

    pthread_mutex_lock(&pool->mutex);

    connection_t* p = primary();
    connection_pool_start_connection_thread(p);

    connection_t* s = supplemental();
    connection_pool_start_connection_thread(s);

    pthread_mutex_unlock(&pool->mutex);

    // Join the primary thread; when it exits, we stop. Supplemental will be joined afterwards.
    if (p && p->thread_running) pthread_join(p->thread, NULL);

    pthread_mutex_lock(&pool->mutex);
    if (s && s->thread_running) {
        s->driver->disconnect_func(true);
        pthread_mutex_unlock(&pool->mutex);
        pthread_join(s->thread, NULL);
        pthread_mutex_lock(&pool->mutex);
        s->thread_running = false;
        s->active = false;
    }
    if (p) { p->thread_running = false; p->active = false; }
    pthread_mutex_unlock(&pool->mutex);
}

device_properties_type* connection_pool_primary_device() {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary();
    device_properties_type* d = p ? p->device : NULL;
    pthread_mutex_unlock(&pool->mutex);
    return d;
}

const device_driver_type* connection_pool_primary_driver() {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary();
    const device_driver_type* d = p ? p->driver : NULL;
    pthread_mutex_unlock(&pool->mutex);
    return d;
}

void connection_pool_handle_device_removed(const char* driver_id) {
    if (config()->debug_connections) log_debug("connection_pool_handle_device_removed for driver %s\n", driver_id);

    pthread_mutex_lock(&pool->mutex);

    connection_t* p = primary();
    bool blocked_on_active = p && p->active && p->thread_running;
    bool primary_removed = false;
    bool supplemental_removed = pool->supplemental_index == -1;

    int remove_index = -1;
    for (int i = 0; i < pool->count; ++i) {
        connection_t* c = pool->list[i];
        if (c && strcmp(c->driver->id, driver_id) == 0) {
            // Request a soft disconnect; the driver threads will exit on their own.
            if (c->driver && c->driver->disconnect_func) c->driver->disconnect_func(false);
            remove_index = i;

            primary_removed = remove_index == pool->primary_index;
            supplemental_removed |= remove_index == pool->supplemental_index;
            if (primary_removed) {
                // we'll be reevaluating selections below, so soft disconnect supplemental
                connection_t* s = supplemental();
                if (s) s->driver->disconnect_func(true);
            }

            break;
        }
    }

    if (remove_index >= 0) {
        connection_t* c = pool->list[remove_index];
        free(c);

        // Remove from array and free the connection wrapper (device is managed externally)
        for (int j = remove_index + 1; j < pool->count; ++j) pool->list[j - 1] = pool->list[j];
        pool->count--;

        // Re-evaluate selections
        pool->primary_index = pick_primary_index(pool);
        if (config()->debug_connections) log_debug("connection_pool_handle_device_removed picked primary %d\n", pool->primary_index);
        pool->supplemental_index = pick_supplemental_index(pool);
        bool supplemental_changed = supplemental_removed && pool->supplemental_index != -1;
        if (blocked_on_active && !primary_removed && supplemental_changed) {
            connection_t* s = supplemental();
            connection_pool_start_connection_thread(s);
        }
        if (config()->debug_connections) log_debug("connection_pool_handle_device_removed picked supplemental %d\n", pool->supplemental_index);

        // Reset time sync when topology changes
        pool->time_sync_initialized = false;
        if (pool->time_sync) { imu_time_sync_destroy(pool->time_sync); pool->time_sync = NULL; }
        if (pool->rate_est_primary) { imu_rate_estimator_destroy(pool->rate_est_primary); pool->rate_est_primary = imu_rate_estimator_create(256); }
        if (pool->rate_est_supp) { imu_rate_estimator_destroy(pool->rate_est_supp); pool->rate_est_supp = imu_rate_estimator_create(256); }
    }

    pthread_mutex_unlock(&pool->mutex);
}

static int index_of_driver_id(const char* driver_id) {
    if (!driver_id) return -1;
    for (int i = 0; i < pool->count; ++i) {
        connection_t* c = pool->list[i];
        if (c && strcmp(c->driver->id, driver_id) == 0) return i;
    }
    return -1;
}

void connection_pool_ingest_imu_quat(const char* driver_id, imu_pose_type pose) {
    pthread_mutex_lock(&pool->mutex);
    int idx = index_of_driver_id(driver_id);
    if (idx < 0) { pthread_mutex_unlock(&pool->mutex); return; }
    int primary_idx = pool->primary_index;
    int supp_idx = pool->supplemental_index;

    // Update rate estimators first
    if (idx == primary_idx) {
        imu_rate_estimator_add(pool->rate_est_primary, pose.timestamp_ms);
    } else if (idx == supp_idx) {
        imu_rate_estimator_add(pool->rate_est_supp, pose.timestamp_ms);
    }

    // Initialize time sync once both rates are known
    if (!pool->time_sync_initialized && primary_idx >= 0 && supp_idx >= 0 &&
        imu_rate_estimator_is_ready(pool->rate_est_primary) &&
        imu_rate_estimator_is_ready(pool->rate_est_supp)) {
        float rate1 = imu_rate_estimator_get_rate_hz(pool->rate_est_primary);
        float rate2 = imu_rate_estimator_get_rate_hz(pool->rate_est_supp);
        // Use a 2s window by default; could be configurable later
        float window_s = 2.0f;
        pool->time_sync = imu_time_sync_create(window_s, rate1, rate2);
        pool->time_sync_initialized = (pool->time_sync != NULL);
        if (config()->debug_connections) log_debug("time_sync initialized (rate1=%.2f, rate2=%.2f)\n", rate1, rate2);
    }

    // Feed samples if initialized
    if (pool->time_sync_initialized && pool->time_sync) {
        int src = (idx == primary_idx) ? 0 : (idx == supp_idx ? 1 : -1);
        if (src >= 0) {
            imu_time_sync_add_quaternion_sample(pool->time_sync, src, pose.orientation);

            // Opportunistically compute offset when ready
            float offset_s, conf;
            if (imu_time_sync_is_ready(pool->time_sync) &&
                imu_time_sync_compute_offset(pool->time_sync, &offset_s, &conf)) {
                pool->last_offset_s = offset_s;
                pool->last_confidence = conf;
                if (config()->debug_connections) {
                    log_debug("time_sync update: offset=%.6fs, confidence=%.3f\n", offset_s, conf);
                }
            }
        }
    }

    pthread_mutex_unlock(&pool->mutex);
}

bool connection_pool_get_time_delta(float* out_offset_seconds, float* out_confidence) {
    pthread_mutex_lock(&pool->mutex);
    bool ok = pool->time_sync_initialized && pool->time_sync && (pool->last_confidence > 0.0f);
    if (ok) {
        if (out_offset_seconds) *out_offset_seconds = pool->last_offset_s;
        if (out_confidence) *out_confidence = pool->last_confidence;
    }
    pthread_mutex_unlock(&pool->mutex);
    return ok;
}

connection_t* connection_pool_find_hid_connection(uint16_t id_vendor, int16_t id_product) {
    if (config()->debug_connections) log_debug("connection_pool_find_hid_connection for vendor %d product %d\n", id_vendor, id_product);
    pthread_mutex_lock(&pool->mutex);
    for (int i = 0; i < pool->count; ++i) {
        connection_t* c = pool->list[i];
        if (c && c->device->hid_vendor_id == id_vendor && c->device->hid_product_id == id_product) {
            pthread_mutex_unlock(&pool->mutex);
            return c;
        }
    }
    pthread_mutex_unlock(&pool->mutex);
    return NULL;
}

connection_t* connection_pool_find_driver_connection(const char* driver_id) {
    if (config()->debug_connections) log_debug("connection_pool_find_driver_connection for driver %s\n", driver_id);
    pthread_mutex_lock(&pool->mutex);
    for (int i = 0; i < pool->count; ++i) {
        connection_t* c = pool->list[i];
        if (c && strcmp(c->driver->id, driver_id) == 0) {
            pthread_mutex_unlock(&pool->mutex);
            return c;
        }
    }
    pthread_mutex_unlock(&pool->mutex);
    return NULL;
}
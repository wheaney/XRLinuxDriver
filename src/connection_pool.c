#include "connection_pool.h"
#include "logging.h"
#include "runtime_context.h"
#include "imu_time_sync.h"
#include "imu.h"

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
    c->ref_set = false;
    c->have_last = false;
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

void connection_pool_ingest_pose(const char* driver_id, imu_pose_type pose) {
    pthread_mutex_lock(&pool->mutex);
    int idx = index_of_driver_id(driver_id);
    if (idx < 0) { pthread_mutex_unlock(&pool->mutex); return; }
    int primary_idx = pool->primary_index;
    int supp_idx = pool->supplemental_index;

    connection_t* conn = pool->list[idx];
    if (conn) {
        conn->last_quat = pose.orientation;
        conn->last_ts_ms = pose.timestamp_ms;
        conn->have_last = true;
    }

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
        float window_s = 5.0f;
        pool->time_sync = imu_time_sync_create(window_s, rate1, rate2);
        pool->time_sync_initialized = (pool->time_sync != NULL);
        if (config()->debug_connections) log_debug("time_sync initialized (rate1=%.2f, rate2=%.2f)\n", rate1, rate2);

        // Capture reference quaternions for both streams based on current sample(s)
        connection_t* p = primary();
        connection_t* s = supplemental();
        if (p && p->have_last) { p->ref_quat = p->last_quat; p->ref_set = true; }
        if (s && s->have_last) { s->ref_quat = s->last_quat; s->ref_set = true; }
    }

    // Feed samples if initialized
    if (pool->time_sync_initialized && pool->time_sync) {
        int src = (idx == primary_idx) ? 0 : (idx == supp_idx ? 1 : -1);
        if (src >= 0) {
            // Convert to relative quaternion using the stream's reference to keep signals comparable
            connection_t* c = pool->list[idx];
            imu_quat_type rel = pose.orientation;
            if (c && c->ref_set) {
                imu_quat_type conj = conjugate(c->ref_quat);
                rel = multiply_quaternions(conj, pose.orientation);
            }
            c->last_rel_quat = rel;
            imu_time_sync_add_quaternion_sample(pool->time_sync, src, rel);

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

    // Decide whether to forward to the driver. We only forward for primary samples to avoid duplicates.
    bool should_forward = (idx == primary_idx);
    imu_quat_type fused_for_forward = pose.orientation;
    if (should_forward) {
        // Prepare fused quaternion under lock using current state
        connection_t* p = primary();
        connection_t* s = supplemental();
        if (p) {
            imu_quat_type fused = p->ref_set ? p->last_rel_quat : p->last_quat;
            if (s && s->have_last && pool->last_confidence > 0.2f) {
                float w = pool->last_confidence;
                if (w < 0.0f) w = 0.0f; if (w > 1.0f) w = 1.0f;
                imu_quat_type q1 = normalize_quaternion(fused);
                imu_quat_type q2 = normalize_quaternion(s->ref_set ? s->last_rel_quat : s->last_quat);
                imu_quat_type blended = {
                    .x = (1.0f - w) * q1.x + w * q2.x,
                    .y = (1.0f - w) * q1.y + w * q2.y,
                    .z = (1.0f - w) * q1.z + w * q2.z,
                    .w = (1.0f - w) * q1.w + w * q2.w,
                };
                fused = normalize_quaternion(blended);
            }
            fused_for_forward = fused;
        }
    }

    pthread_mutex_unlock(&pool->mutex);

    // Forward outside of the lock to avoid deadlocks with driver code paths
    if (should_forward) {
        extern void driver_handle_pose(const char* driver_id, imu_pose_type pose);
        pose.orientation = fused_for_forward;
        driver_handle_pose(driver_id, pose);
    }
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

bool connection_pool_is_primary_driver_id(const char* driver_id) {
    if (!driver_id) return false;
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary();
    bool is_primary = p && strcmp(p->driver->id, driver_id) == 0;
    pthread_mutex_unlock(&pool->mutex);
    return is_primary;
}

// Very simple fusion: if supplemental exists and time sync confidence is available, we can
// blend orientations; for now, return primary rel quaternion, optionally nudged by supplemental.
// This is a placeholder for more sophisticated fusion.
bool connection_pool_get_fused_quaternion(uint32_t timestamp_ms, imu_quat_type* out_quat) {
    (void)timestamp_ms;
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary();
    if (!p || !p->have_last) { pthread_mutex_unlock(&pool->mutex); return false; }

    imu_quat_type fused = p->ref_set ? p->last_rel_quat : p->last_quat;

    connection_t* s = supplemental();
    if (s && s->have_last && pool->last_confidence > 0.2f) {
        // Simple slerp-ish linear blend in quaternion space (not true slerp; acceptable as placeholder)
        // weight based on confidence, clamped
        float w = pool->last_confidence;
        if (w < 0.0f) w = 0.0f; if (w > 1.0f) w = 1.0f;
        imu_quat_type q1 = fused;
        imu_quat_type q2 = s->ref_set ? s->last_rel_quat : s->last_quat;
        // normalize inputs
        q1 = normalize_quaternion(q1);
        q2 = normalize_quaternion(q2);
        imu_quat_type blended = {
            .x = (1.0f - w) * q1.x + w * q2.x,
            .y = (1.0f - w) * q1.y + w * q2.y,
            .z = (1.0f - w) * q1.z + w * q2.z,
            .w = (1.0f - w) * q1.w + w * q2.w,
        };
        fused = normalize_quaternion(blended);
    }
    *out_quat = fused;
    pthread_mutex_unlock(&pool->mutex);
    return true;
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
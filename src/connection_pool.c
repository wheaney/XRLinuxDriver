#include "connection_pool.h"
#include "logging.h"
#include "runtime_context.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static connection_pool_type* pool = NULL;

static void ensure_capacity() {
    if (config()->debug_connections) log_debug("ensure_capacity\n");
    if (pool->count >= pool->capacity) {
        int newcap = pool->capacity == 0 ? 2 : pool->capacity + 1;
        pool->list = (connection_t**)realloc(pool->list, newcap * sizeof(connection_t*));
        pool->capacity = newcap;
    }
}

void connection_pool_init() {
    if (config()->debug_connections) log_debug("connection_pool_init\n");
    pool = (connection_pool_type*)calloc(1, sizeof(*pool));
    pthread_mutex_init(&pool->mutex, NULL);
    pool->primary_index = -1;
    pool->supplemental_index = -1;
}

static int pick_primary_index() {
    if (config()->debug_connections) log_debug("pick_primary_index\n");
    for (int i = 0; i < pool->count; ++i) {
        if (!pool->list[i]->device->can_be_supplemental) return i;
    }
    return pool->count > 0 ? 0 : -1;
}

static int pick_supplemental_index() {
    if (config()->debug_connections) log_debug("pick_supplemental_index\n");
    for (int i = 0; i < pool->count; ++i) {
        if (i != pool->primary_index && pool->list[i]->supplemental) return i;
    }
    return -1;
}

// The pool mutex must already be held when calling these helpers.
static connection_t* find_hid_connection_locked(uint16_t id_vendor, int16_t id_product) {
    for (int i = 0; i < pool->count; ++i) {
        connection_t* c = pool->list[i];
        if (c && c->device && c->device->hid_vendor_id == id_vendor && c->device->hid_product_id == id_product) return c;
    }
    return NULL;
}

static int find_driver_connection_index_locked(const char* driver_id) {
    for (int i = 0; i < pool->count; ++i) {
        connection_t* c = pool->list[i];
        if (c && c->driver && c->driver->id && driver_id && strcmp(c->driver->id, driver_id) == 0) return i;
    }
    return -1;
}

static connection_t* find_driver_connection_locked(const char* driver_id) {
    int idx = find_driver_connection_index_locked(driver_id);
    return idx >= 0 ? pool->list[idx] : NULL;
}

void connection_pool_handle_device_added(const device_driver_type* driver, device_properties_type* device) {
    if (config()->debug_connections) log_debug("connection_pool_handle_device_added for driver %s\n", driver->id);

    pthread_mutex_lock(&pool->mutex);
    if (device && find_hid_connection_locked(device->hid_vendor_id, device->hid_product_id)) {
        if (config()->debug_connections) {
            log_debug(
                "connection_pool_handle_device_added: ignoring duplicate HID device for driver %s (0x%04x:0x%04x)\n",
                driver ? driver->id : "(null)",
                (unsigned int)device->hid_vendor_id,
                (unsigned int)device->hid_product_id);
        }
        free(device);
        pthread_mutex_unlock(&pool->mutex);
        return;
    }

    ensure_capacity(pool);
    connection_t* c = (connection_t*)calloc(1, sizeof(*c));
    c->driver = driver;
    c->device = device; // take ownership
    c->supplemental = device->can_be_supplemental;
    c->active = false;
    pool->list[pool->count++] = c;

    // If no primary selected yet, pick one
    if (pool->primary_index == -1) {
        pool->primary_index = pick_primary_index(pool);
        if (config()->debug_connections) log_debug("connection_pool_handle_device_added picked primary %d\n", pool->primary_index);
    }
    // If possible, choose a supplemental distinct from primary
    if (pool->supplemental_index == -1) {
        pool->supplemental_index = pick_supplemental_index(pool);
        if (config()->debug_connections) log_debug("connection_pool_handle_device_added picked supplemental %d\n", pool->supplemental_index);
    }

    pthread_mutex_unlock(&pool->mutex);
}

static connection_t* primary() {
    if (pool->primary_index < 0 || pool->primary_index >= pool->count) return NULL;
    return pool->list[pool->primary_index];
}

static connection_t* supplemental() {
    if (pool->supplemental_index < 0 || pool->supplemental_index >= pool->count) return NULL;
    return pool->list[pool->supplemental_index];
}

bool connection_pool_is_connected() {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary(pool);
    bool connected = p && p->driver->is_connected_func();
    pthread_mutex_unlock(&pool->mutex);
    return connected;
}

bool connection_pool_device_is_sbs_mode() {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary(pool);
    bool enabled = p && p->driver->device_is_sbs_mode_func();
    pthread_mutex_unlock(&pool->mutex);
    return enabled;
}

bool connection_pool_device_set_sbs_mode(bool enabled) {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary(pool);
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
    pthread_mutex_unlock(&pool->mutex);
}

static void* block_thread_func(void* arg) {
    connection_t* c = (connection_t*)arg;
    if (config()->debug_connections) log_debug("block_thread_func %s\n", c->driver->id);
    c->driver->block_on_device_func();
    c->thread_running = false;
    return NULL;
}

bool connection_pool_connect_active() {
    if (config()->debug_connections) log_debug("connection_pool_connect_active\n");
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary(pool);
    connection_t* s = supplemental(pool);
    pthread_mutex_unlock(&pool->mutex);

    bool pr_ok = false;
    if (p) pr_ok = p->driver->device_connect_func();
    if (s) (void)s->driver->device_connect_func();
    return pr_ok;
}

void connection_pool_block_on_active() {
    if (config()->debug_connections) log_debug("connection_pool_block_on_active\n");
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary(pool);
    connection_t* s = supplemental(pool);

    if (p && !p->thread_running) {
        p->active = true;
        p->thread_running = true;
        pthread_create(&p->thread, NULL, block_thread_func, p);
    }
    if (s && !s->thread_running) {
        s->active = true;
        s->thread_running = true;
        pthread_create(&s->thread, NULL, block_thread_func, s);
    }
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
    connection_t* p = primary(pool);
    device_properties_type* d = p ? p->device : NULL;
    pthread_mutex_unlock(&pool->mutex);
    return d;
}

const device_driver_type* connection_pool_primary_driver() {
    pthread_mutex_lock(&pool->mutex);
    connection_t* p = primary(pool);
    const device_driver_type* d = p ? p->driver : NULL;
    pthread_mutex_unlock(&pool->mutex);
    return d;
}

void connection_pool_handle_device_removed(const char* driver_id) {
    if (config()->debug_connections) log_debug("connection_pool_handle_device_removed for driver %s\n", driver_id);

    pthread_mutex_lock(&pool->mutex);

    int remove_index = find_driver_connection_index_locked(driver_id);
    if (remove_index >= 0) {
        connection_t* c = pool->list[remove_index];
        // Request a soft disconnect; the driver threads will exit on their own.
        if (c && c->driver && c->driver->disconnect_func) c->driver->disconnect_func(false);
    }

    if (remove_index >= 0) {
        if (remove_index == pool->primary_index) {
            // we'll be reevaluating selections below, so soft disconnect supplemental
            connection_t* s = supplemental(pool);
            if (s) s->driver->disconnect_func(true);
        }

        connection_t* c = pool->list[remove_index];
        free(c);

        // Remove from array and free the connection wrapper (device is managed externally)
        for (int j = remove_index + 1; j < pool->count; ++j) pool->list[j - 1] = pool->list[j];
        pool->count--;

        // Re-evaluate selections
        pool->primary_index = pick_primary_index(pool);
        if (config()->debug_connections) log_debug("connection_pool_handle_device_removed picked primary %d\n", pool->primary_index);
        pool->supplemental_index = pick_supplemental_index(pool);
        if (config()->debug_connections) log_debug("connection_pool_handle_device_removed picked supplemental %d\n", pool->supplemental_index);
    }

    pthread_mutex_unlock(&pool->mutex);
}

connection_t* connection_pool_find_hid_connection(uint16_t id_vendor, int16_t id_product) {
    if (config()->debug_connections) log_debug("connection_pool_find_hid_connection for vendor %d product %d\n", id_vendor, id_product);
    pthread_mutex_lock(&pool->mutex);
    connection_t* c = find_hid_connection_locked(id_vendor, id_product);
    pthread_mutex_unlock(&pool->mutex);
    return c;
}

connection_t* connection_pool_find_driver_connection(const char* driver_id) {
    if (config()->debug_connections) log_debug("connection_pool_find_driver_connection for driver %s\n", driver_id);
    pthread_mutex_lock(&pool->mutex);
    connection_t* c = find_driver_connection_locked(driver_id);
    pthread_mutex_unlock(&pool->mutex);
    return c;
}
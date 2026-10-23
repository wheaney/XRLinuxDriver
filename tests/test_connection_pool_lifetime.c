#include "runtime_context.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

runtime_context g_runtime_context;
void log_debug(const char *format, ...) { (void)format; }
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static bool entered, released;
static bool fake_connect(void) { return true; }
static bool fake_sbs(void) { return false; }
static bool fake_set_sbs(bool value) { (void)value; return true; }
static void fake_disconnect(bool soft) { (void)soft; }
static void fake_block(void) {
    pthread_mutex_lock(&gate);
    entered = true;
    pthread_cond_broadcast(&changed);
    while (!released) pthread_cond_wait(&changed, &gate);
    pthread_mutex_unlock(&gate);
}
static const device_driver_type driver = {
    .id = "test",
    .device_connect_func = fake_connect,
    .block_on_device_func = fake_block,
    .device_is_sbs_mode_func = fake_sbs,
    .device_set_sbs_mode_func = fake_set_sbs,
    .is_connected_func = fake_connect,
    .disconnect_func = fake_disconnect,
};
static void *outer(void *unused) {
    (void)unused;
    connection_pool_block_on_active();
    return NULL;
}
int main(void) {
    driver_config_type cfg = {0};
    set_config(&cfg);
    connection_pool_init(NULL, NULL);
    device_properties_type *device = calloc(1, sizeof(*device));
    assert(device);
    device->hid_vendor_id = 0x35ca;
    device->hid_product_id = 0x1104;
    connection_pool_handle_device_added(&driver, device);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, outer, NULL) == 0);
    pthread_mutex_lock(&gate);
    while (!entered) pthread_cond_wait(&changed, &gate);
    pthread_mutex_unlock(&gate);
    connection_pool_handle_device_removed("test");
    pthread_mutex_lock(&gate);
    released = true;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&gate);
    assert(pthread_join(thread, NULL) == 0);
    free(device);
    return 0;
}

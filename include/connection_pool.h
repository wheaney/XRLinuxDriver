#pragma once

#include "devices.h"
#include "imu.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct connection_t {
    const device_driver_type* driver;
    device_properties_type* device; // owned by pool
    bool supplemental;
    bool active;
    pthread_t thread;
    bool thread_running;
} connection_t;

struct connection_pool_t {
    pthread_mutex_t mutex;
    connection_t** list;
    int count;
    int capacity;
    int primary_index;      // index in list or -1
    int supplemental_index; // index in list or -1
};

// Connection pool type that manages multiple device connections.
typedef struct connection_pool_t connection_pool_type;

// Create/destroy a connection pool instance
typedef void (*pose_handler_t)(imu_pose_type pose);
void connection_pool_init(pose_handler_t pose_handler_callback);

// Append a new connection (driver + device). The pool retains the driver pointer and
// takes ownership of the device pointer. It will decide whether to make it the primary
// (non-supplemental) or use it as a supplemental connection.
void connection_pool_handle_device_added(const device_driver_type* driver, device_properties_type* device);

// Delegate helpers the main driver uses (these generally forward to the primary connection)
bool connection_pool_is_connected();
bool connection_pool_device_is_sbs_mode();
bool connection_pool_device_set_sbs_mode(bool enabled);
void connection_pool_disconnect_all(bool soft);

// Start blocking on active connections (primary and at most one supplemental). This function
// will create per-connection threads and return when the primary connection stops blocking
// (e.g., due to disconnect).
void connection_pool_block_on_active();

// Attempt to connect active connections (primary and, if present, supplemental). Returns true
// if the primary connection connected successfully (supplemental is best-effort).
bool connection_pool_connect_active();

// Get the current primary device properties (borrowed; do not free). Returns NULL if none.
device_properties_type* connection_pool_primary_device();

// Get the primary driver pointer (borrowed; do not free). Returns NULL if none.
const device_driver_type* connection_pool_primary_driver();

// Remove a connection by its unique source id. If the removed connection is currently
// primary or supplemental, the pool will re-evaluate selection. If a thread is running for the
// removed connection, it will be disconnected; cleanup occurs when the thread exits.
void connection_pool_handle_device_removed(const char* driver_id);

connection_t* connection_pool_find_hid_connection(uint16_t id_vendor, int16_t id_product);
connection_t* connection_pool_find_driver_connection(const char* driver_id);

void connection_pool_ingest_pose(const char* driver_id, imu_pose_type pose);

// Returns true if the given driver id is currently the primary connection
bool connection_pool_is_primary_driver_id(const char* driver_id);
#include "epoch.h"

#include <time.h>

struct timespec ts;
uint64_t get_epoch_time_ms() {
    timespec_get(&ts, TIME_UTC);

    long int sec_ms = ts.tv_sec * 1000;
    long int nsec_ms = ts.tv_nsec / 1000000;

    return (uint64_t)(sec_ms + nsec_ms);
}
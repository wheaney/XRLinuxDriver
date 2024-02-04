#include <curl/curl.h>
#include <pthread.h>

static pthread_mutex_t curl_lock = PTHREAD_MUTEX_INITIALIZER;
static int ref_count = 0;

void curl_init() {
    pthread_mutex_lock(&curl_lock);
    if(ref_count == 0) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    ref_count++;
    pthread_mutex_unlock(&curl_lock);
}

void curl_cleanup() {
    pthread_mutex_lock(&curl_lock);
    ref_count--;
    if(ref_count == 0) {
        curl_global_cleanup();
    }
    pthread_mutex_unlock(&curl_lock);
}

#include "ipc.h"
#include "logging.h"

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char *sombrero_ipc_file_prefix = "/tmp/shader_runtime_";

const char *display_res_ipc_name = "display_resolution";
const char *disabled_ipc_name = "disabled";
const char *date_ipc_name = "keepalive_date";
const char *pose_orientation_ipc_name = "pose_orientation";
const char *pose_data_mutex_ipc_name = "pose_data_mutex";
const char *pose_position_ipc_name = "pose_position";

// deprecated - can be removed once this version is widely distributed
const char *display_fov_ipc_name = "display_fov";
const char *lens_distance_ratio_ipc_name = "lens_distance_ratio";

bool setup_ipc_values(ipc_values_type *ipc_values, bool debug) {
    setup_ipc_value(display_res_ipc_name, (void**) &ipc_values->display_res, sizeof(float) * 2, debug);
    setup_ipc_value(disabled_ipc_name, (void**) &ipc_values->disabled, sizeof(bool), debug);
    setup_ipc_value(date_ipc_name, (void**) &ipc_values->date, sizeof(float) * 4, debug);
    setup_ipc_value(pose_orientation_ipc_name, (void**) &ipc_values->pose_orientation, sizeof(float) * 16, debug);
    setup_ipc_value(pose_position_ipc_name, (void**) &ipc_values->pose_position, sizeof(float) * 3, debug);

    setup_ipc_value(display_fov_ipc_name, (void**) &ipc_values->display_fov, sizeof(float), debug);
    setup_ipc_value(lens_distance_ratio_ipc_name, (void**) &ipc_values->lens_distance_ratio, sizeof(float), debug);

    // attempt to destroy the mutex if it already existed from a previous run
    setup_ipc_value(pose_data_mutex_ipc_name, (void**) &ipc_values->pose_data_mutex, sizeof(pthread_mutex_t), debug);
    int ret = pthread_mutex_destroy(ipc_values->pose_data_mutex);
    if (ret != 0) {
        perror("pthread_mutex_destroy");
        if (ret != EINVAL) return false;
    }

    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        perror("pthread_mutexattr_init");
        return false;
    }
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        perror("pthread_mutexattr_setpshared");
        return false;
    }
    if (pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) != 0) {
        perror("pthread_mutexattr_setrobust");
        return false;
    }
    if (pthread_mutex_init(ipc_values->pose_data_mutex, &attr) != 0) {
        perror("pthread_mutex_init");
        return false;
    }

    return true;
}

void setup_ipc_value(const char *name, void **shmemValue, size_t size, bool debug) {
    char *path = malloc(strlen(sombrero_ipc_file_prefix) + strlen(name) + 1);
    strcpy(path, sombrero_ipc_file_prefix);
    strcat(path, name);

    mode_t old_umask = umask(0);
    int fd = open(path, O_CREAT, 0666);
    if (fd == -1) {
        log_error("Could not create IPC shared file\n");
        exit(1);
    }
    close(fd);
    umask(old_umask);

    key_t key = ftok(path, 0);
    if (debug) log_debug("ipc_key, got key %d for path %s\n", key, path);
    free(path);

    int shmid = shmget(key, size, 0666|IPC_CREAT);
    if (shmid == -1) {
        // it may have been allocated using a different size, attempt to find and delete it
        shmid = shmget(key, 0, 0);
        if (shmid != -1) {
            if (debug) log_debug("ipc_key, deleting shared memory segment with key %d\n", key);
            shmctl(shmid, IPC_RMID, NULL);
        } else {
            if (debug) log_debug("ipc_key, couldn't delete, no shmid for key %d\n", key);
        }
    }

    if (shmid != -1) {
        *shmemValue = shmat(shmid,(void*)0,0);
        if (*shmemValue == (void *) -1) {
            log_error("Error calling shmat\n");
            exit(1);
        }
    } else {
        log_error("Error calling shmget\n");
        exit(1);
    }
}

void cleanup_ipc(char* file_prefix, bool debug) {
    if (debug) log_debug("cleanup_ipc, disabling IPC\n");
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s*", file_prefix);

    glob_t glob_result;
    glob(pattern, GLOB_TILDE, NULL, &glob_result);

    for(unsigned int i=0; i<glob_result.gl_pathc; ++i){
        char* file = glob_result.gl_pathv[i];
        key_t key = ftok(file, 0);
        int shmid = shmget(key, 0, 0);
        if (shmid != -1) {
            if (debug) log_debug("cleanup_ipc, deleting shared memory segment with key %d\n", key);
            shmctl(shmid, IPC_RMID, NULL);
        } else {
            if (debug) log_debug("cleanup_ipc, couldn't delete, no shmid for key %d\n", key);
        }
    }

    globfree(&glob_result);
}
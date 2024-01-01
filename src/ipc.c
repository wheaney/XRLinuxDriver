#include "ipc.h"

#include <errno.h>
#include <glob.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

const char *sombrero_ipc_file_prefix = "/tmp/shader_runtime_";

const char *display_res_ipc_name = "display_res";
const char *display_fov_ipc_name = "display_fov";
const char *lens_distance_ratio_ipc_name = "lens_distance_ratio";
const char *disabled_ipc_name = "disabled";
const char *date_ipc_name = "keepalive_date";

bool setup_ipc_values(ipc_values_type *ipc_values, bool debug) {
    setup_ipc_value(display_res_ipc_name, (void**) &ipc_values->display_res, sizeof(unsigned int) * 2, debug);
    setup_ipc_value(display_fov_ipc_name, (void**) &ipc_values->display_fov, sizeof(float), debug);
    setup_ipc_value(lens_distance_ratio_ipc_name, (void**) &ipc_values->lens_distance_ratio, sizeof(float), debug);
    setup_ipc_value(disabled_ipc_name, (void**) &ipc_values->disabled, sizeof(bool), debug);
    setup_ipc_value(date_ipc_name, (void**) &ipc_values->date, sizeof(float) * 4, debug);

    return true;
}

void setup_ipc_value(const char *name, void **shmemValue, size_t size, bool debug) {
    char *path = malloc(strlen(sombrero_ipc_file_prefix) + strlen(name) + 1);
    strcpy(path, sombrero_ipc_file_prefix);
    strcat(path, name);

    FILE *ipc_file = fopen(path, "w");
    if (ipc_file == NULL) {
        fprintf(stderr, "Could not create IPC shared file\n");
        exit(1);
    }
    fclose(ipc_file);

    key_t key = ftok(path, 0);
    if (debug) printf("\tdebug: ipc_key, got key %d for path %s\n", key, path);
    free(path);

    int shmid = shmget(key, size, 0666|IPC_CREAT);
    if (shmid == -1) {
        // it may have been allocated using a different size, attempt to find and delete it
        shmid = shmget(key, 0, 0);
        if (shmid != -1) {
            if (debug) printf("\tdebug: ipc_key, deleting shared memory segment with key %d\n", key);
            shmctl(shmid, IPC_RMID, NULL);
        } else {
            if (debug) printf("\tdebug: ipc_key, couldn't delete, no shmid for key %d\n", key);
        }
    }

    if (shmid != -1) {
        *shmemValue = shmat(shmid,(void*)0,0);
        if (*shmemValue == (void *) -1) {
            fprintf(stderr, "Error calling shmat\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "Error calling shmget\n");
        exit(1);
    }
}

void cleanup_ipc(char* file_prefix, bool debug) {
    if (debug) printf("\tdebug: cleanup_ipc, disabling IPC\n");
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s*", file_prefix);

    glob_t glob_result;
    glob(pattern, GLOB_TILDE, NULL, &glob_result);

    for(unsigned int i=0; i<glob_result.gl_pathc; ++i){
        char* file = glob_result.gl_pathv[i];
        key_t key = ftok(file, 0);
        int shmid = shmget(key, 0, 0);
        if (shmid != -1) {
            if (debug) printf("\tdebug: cleanup_ipc, deleting shared memory segment with key %d\n", key);
            shmctl(shmid, IPC_RMID, NULL);
        } else {
            if (debug) printf("\tdebug: cleanup_ipc, couldn't delete, no shmid for key %d\n", key);
        }
    }

    globfree(&glob_result);
}
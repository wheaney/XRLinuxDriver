#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <glob.h>

#include "ipc.h"

char *ipc_file_prefix = NULL;

void set_ipc_file_prefix(char *new_prefix) {
    ipc_file_prefix = new_prefix;
}

char *get_ipc_file_prefix() {
    return ipc_file_prefix;
}

void setup_ipc_value(const char *name, void **shmemValue, size_t size, bool debug) {
    char *path = malloc(strlen(ipc_file_prefix) + strlen(name) + 1);
    strcpy(path, ipc_file_prefix);
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
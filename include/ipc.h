#include <stdbool.h>
#include <stdio.h>

void set_ipc_file_prefix(char *new_prefix);

char *get_ipc_file_prefix();

void setup_ipc_value(const char *name, void **shmemValue, size_t size, bool debug);

void cleanup_ipc(char* file_prefix, bool debug);
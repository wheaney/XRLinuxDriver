#pragma once

#include <stdio.h>
#include <stdbool.h>

// creates a file, if it doesn't already exist, in the user home directory with home directory permissions and ownership.
// this is helpful since the driver may be run with sudo, so we don't create files owned by root:root
FILE* get_or_create_home_file(char *filename, char *mode, char *full_path, bool *created);
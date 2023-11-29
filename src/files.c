#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

FILE* get_or_create_home_file(char *filename, char *mode, char *full_path, bool *created) {
    char *home_directory = getenv("HOME");
    snprintf(full_path, 1024, "%s/%s", home_directory, filename);
    FILE *fp = fopen(full_path, mode ? mode : "r");
    if (fp == NULL) {
        // Retrieve the permissions of the parent directory
        struct stat st;
        if (stat(home_directory, &st) == -1) {
            perror("stat");
            return NULL;
        }

        fp = fopen(full_path, "w");
        if (fp == NULL) {
            perror("Error creating config file");
            return NULL;
        }
        if (created != NULL)
            *created = true;

        // Set the permissions and ownership of the new file to be the same as the parent directory
        if (chmod(full_path, st.st_mode & 0777) == -1) {
            perror("Error setting file permissions");
            return NULL;
        }
        if (chown(full_path, st.st_uid, st.st_gid) == -1) {
            perror("Error setting file ownership");
            return NULL;
        }
    } else if (created != NULL) {
        *created = false;
    }

    return fp;
}
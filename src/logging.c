#include "files.h"
#include "logging.h"
#include "state.h"
#include "strings.h"
#include "version.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

void log_init() {
    // ensure the log file exists, reroute stdout and stderr there
    char *log_file_path = NULL;
    FILE *log_file = get_or_create_state_file("driver.log", NULL, &log_file_path, NULL);
    fclose(log_file);
    freopen(log_file_path, "a", stdout);
    freopen(log_file_path, "a", stderr);
    free_and_clear(&log_file_path);

    // when redirecting stdout/stderr to a file, it becomes fully buffered, requiring lots of manual flushing of the
    // stream, this makes them unbuffered, which is fine since we log so little
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    log_message("Project version: %s\n", PROJECT_VERSION);
}

static void do_log(const char* prefix, const char* format, va_list args) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);
    printf("%04d-%02d-%02d %02d:%02d:%02d.%03d %s",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec / 1000), prefix);

    vprintf(format, args);
}

void log_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    do_log("", format, args);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    do_log("[ERROR] ", format, args);
    va_end(args);
}

void log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    do_log("[DEBUG] ", format, args);
    va_end(args);
}
#pragma once

void log_init();
void log_message(const char* format, ...);
void log_error(const char* format, ...);
void log_debug(const char* format, ...);
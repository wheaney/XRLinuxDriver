#pragma once

#define free_and_clear(ptr) do { \
	if (ptr != NULL && *ptr != NULL) { \
		free(*ptr); \
		*ptr = NULL; \
	} \
} while(0)
//
// Created by admn on 11/13/26.
//

#ifndef ASM_ERROR_H
#define ASM_ERROR_H

#include <stdio.h>
#define PRINT_ERROR(msg) print_error((msg), __FILE__, __LINE__)

typedef enum {
    OK = 0,
    LIST_ERR_ADDING_NEW_NODE = -1,
    MEMORY_ALLOCATION_ERROR = -2,
    LIST_ERR_INVALID_ARG = -3,
    LIST_ERR_INVALID_NODE = -4,
} error_status;

static inline void print_error(const char *msg, const char *file_name, const int file_line) {
    fprintf(stderr, "%s\n", msg);
    fprintf(stderr, "File name: %s. Line number: %d\n", file_name, file_line);
}

#endif

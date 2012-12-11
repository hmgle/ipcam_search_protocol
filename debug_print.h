#ifndef _DEBUG_PRINT_H
#define _DEBUG_PRINT_H

#include <stdio.h>

#define DEBUG_PRINT         1
#define debug_print(fmt, ...) \
    do { \
        if (DEBUG_PRINT) \
            fprintf(stderr, "debug_print: %s: %d: %s():" \
                    fmt "\n", __FILE__, __LINE__, __func__, \
                    ##__VA_ARGS__); \
    } while (0)

#endif

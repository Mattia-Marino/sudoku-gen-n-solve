#include <stdio.h>
#include "colors.h"

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define dprint(fmt, ...) \
        do { if (DEBUG_TEST) fprintf(stderr, ANSI_COLOR_BLUE "%s:%d:%s(): " fmt ANSI_COLOR_RESET, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

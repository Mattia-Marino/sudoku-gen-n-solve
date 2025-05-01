/* SPDX-License-Identifier: GPL-3.0 */

#ifndef DEBUG_H
#define DEBUG_H

/* Include necessary headers only when DEBUG is defined */
#ifdef DEBUG
#include <stdio.h>
#include "linked_list.h"
#include "solver.h"

/* DPRINTF: Prints debug messages */
#define DPRINTF(...) printf(__VA_ARGS__)

/* DPRINT_LIST: Prints the linked list */
#define DPRINT_LIST(...) print_list(__VA_ARGS__)

/* DPRINT_EXTENDED_GRID: Prints the extended grid representing
 * the sudoku puzzle with all the possible candidates.
 */
#define DPRINT_EXTENDED_GRID(...) print_extended_grid(__VA_ARGS__)

#else

/* Define macros as empty statements when DEBUG is not defined */
#define DPRINTF(...) do {} while (0)
#define DPRINT_LIST(...) do {} while (0)
#define DPRINT_EXTENDED_GRID(...) do {} while (0)

#endif

#endif /* DEBUG_H */

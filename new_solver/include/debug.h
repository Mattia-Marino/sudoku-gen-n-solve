/* SPDX-License-Identifier: GPL-3.0 */

#ifndef DEBUG_H
#define DEBUG_H

/* Include necessary headers only when DEBUG is defined */
#ifdef DEBUG
#include <stdio.h>
#include "colors.h"
// #include "solver.h"
// #include "sudoku_utils.h"

/* DPRINTF: Prints debug messages with color */
#define DPRINTF(...) printf(ANSI_COLOR_CYAN); printf(__VA_ARGS__); printf(ANSI_COLOR_RESET)

/* DPRINT_SUDOKU: Prints the sudoku grid */
#define DPRINT_SUDOKU(...) display_sudoku(__VA_ARGS__)

/* DPRINT_BOARD: Print the sudoku board showing the bitarrays */
#define DPRINT_BOARD(...) print_board(__VA_ARGS__)

#else

/* Define macros as empty statements when DEBUG is not defined */
#define DPRINTF(...) do {} while (0)
#define DPRINT_SUDOKU(...) do {} while (0)
#define DPRINT_BOARD(...) do {} while (0)

#endif

#endif /* DEBUG_H */
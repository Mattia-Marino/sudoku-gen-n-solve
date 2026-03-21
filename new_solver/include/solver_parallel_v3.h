#ifndef P_SOLVER_H_V3
#define P_SOLVER_H_V3

#include "monitors.h"

struct cell {
    int value;
    int has_value;
    int candidates;
    struct RW_monitor *cs_monitor;
    struct RW_monitor *val_monitor;
    
};

struct board {
	int unset_cells;
    struct RW_monitor *counter_monitor;
	struct cell **cells;
};

struct cords{
    int x;
    int y;
};

struct coordinates {
	int row;
	int column;
};

struct naked_masks{
    int h_single;
    int h_pair;
    int n_pair;
};

int sudoku_solver(int **grid, int n);
#endif


#ifndef SOLVER_H_V4
#define SOLVER_H_V4
#include "pthread_groups.h"

struct cell {
    int value;
    int has_value;
    int candidates;
};

struct board {
	int unset_cells;
    pthread_mutex_t change_lock;
    int has_changed;
	struct cell **cells;
    pthread_barrier_t step_barrier;
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

struct ThreadGroup* parallel_sudoku_solver(int **grid, int n);
int check_sudoku_solved(struct ThreadGroup*);
#endif

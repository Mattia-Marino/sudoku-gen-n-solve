#ifndef P_SOLVER_H_V3
#define P_SOLVER_H_V3
#include "monitors.h"
#include "pthread_groups.h"

struct cell {
    int value;
    int has_value;
    int candidates;
    struct RW_monitor *cs_monitor;
    struct RW_monitor *val_monitor;
    
};

struct board {
	int unset_cells;
    pthread_mutex_t change_lock;
    int has_changed;
    struct RW_monitor *counter_monitor;
    pthread_barrier_t investigation_barrier;
    pthread_barrier_t freeze_barrier;
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

struct ThreadGroup* parallel_sudoku_solver(int **grid, int n);
int check_sudoku_solved(struct ThreadGroup*);
#endif

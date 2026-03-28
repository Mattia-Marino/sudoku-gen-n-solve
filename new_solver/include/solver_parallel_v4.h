
#ifndef SOLVER_H_V3
#define SOLVER_H_V3

struct cell {
    int value;
    int has_value;
    int candidates;
};

struct board {
	int unset_cells;
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

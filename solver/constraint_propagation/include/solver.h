#ifndef SOLVER_H
#define SOLVER_H
struct coordinates {
	int row;
	int column;
};

int sudoku_solver(int **grid, int n);

struct node ***extend_grid(int **grid, int n);

void initialize_propagation_matrix(int **matrix, int n);

/* Naked candidates */

int naked_candidates_rows(struct node ***extended_grid, int n,
			  int **already_propagated, int depth);

int naked_candidates_columns(struct node ***extended_grid, int n,
			     int **already_propagated, int depth);

int naked_candidates_boxes(struct node ***extended_grid, int n,
			   int **already_propagated, int depth);

/* Propagations for naked candidates */

void propagate_row(struct node ***extended_grid, int n,
		   struct coordinates *coord, int n_coordinates, int value);

void propagate_column(struct node ***extended_grid, int n,
		      struct coordinates *coord, int n_coordinates, int value);

void propagate_box(struct node ***extended_grid, int n,
		   struct coordinates *coord, int n_coordinates, int value);

/*Hidden singles*/

int hidden_singles(struct node ***extended_grid, int n);

int check_hidden_single(struct node ***extended_grid, int n, int row, int col,
			int value);

void print_extended_grid(struct node ***extended_grid, int n);

void free_extended_grid(struct node ***extended_grid, int n);

void free_propagation_matrix(int ***propagation, int n);

#endif /* SOLVER_H */


/**
 * Structure representing coordinates in a 2D grid.
 */
struct coordinates {
	int row;	/* Row index */
	int column;	/* Column index */
};

int sudoku_solver(int **grid, int n);
struct node ***extend_grid(int **grid, int n);
void print_extended_grid(struct node ***extended_grid, int n);
void propagate(struct node ***extended_grid, int n, struct coordinates *coord, int n_coord, int value);
int simple_elimination(struct node ***extended_grid, int n, int **already_propagated, int depth);
int hidden_singles(struct node ***extended_grid, int n);
int check_hidden_single(struct node ***extended_grid, int n, int row, int col, int value);
int naked_pair(struct node ***extended_grid, int n);
int check_naked_pair(struct node ***extended_grid, int n, int row, int col);
void initialize_propagation_grid(int ***propagation, int n);
void free_extended_grid(struct node ***extended_grid, int n);
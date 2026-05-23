#ifndef SOLVER_PARALLEL_V5_H
#define SOLVER_PARALLEL_V5_H

/*
 * Sense-reversing spin barrier. Used in place of pthread_barrier_t
 * because the cooperative solver hits 4 barriers per convergence
 * iteration on ~30us puzzles -- futex-based pthread barriers add
 * microseconds of overhead per wait, which alone exceeds the work
 * being coordinated. A spin barrier on 3 pinned threads takes only
 * the time of a couple of cache-line bounces.
 *
 * Each thread keeps its own local_sense bit (starting at 0) and
 * passes it by pointer to spin_barrier_wait. All threads in the
 * group must execute the same number of waits to stay in lockstep.
 */
struct spin_barrier {
	volatile int counter;
	volatile int sense;
	int n;
};

struct cell {
	int value;
	int has_value;
	int candidates;
};

struct board {
	int unset_cells;
	int has_changed;
	struct cell **cells;
	struct spin_barrier *step_barrier;
};

struct cords {
	int x;
	int y;
};

struct naked_masks {
	int h_single;
	int h_pair;
	int n_pair;
};

void spin_barrier_init(struct spin_barrier *b, int n);
void spin_barrier_wait(struct spin_barrier *b, int *local_sense);

/*
 * Boards are allocated once per threadgroup and reused across all
 * the puzzles the group consumes. reset_and_populate wipes the
 * cell state and loads the next grid in a single pass.
 */
struct board *init_board(struct spin_barrier *barrier);
void reset_and_populate(struct board *b, int **grid);
void board_to_grid(const struct board *b, int **grid);
void free_board(struct board *b);

/*
 * Per-thread cooperative solver. thread_id is 1-based and tg_size
 * is in [1, 3]. All threads in the group call this concurrently
 * with the same board and tg_size, synchronizing on
 * b->step_barrier (initialized with tg_size participants). The
 * function returns when the board has converged.
 */
void solve_board_thread(struct board *b, int thread_id, int tg_size,
			int *local_sense);

#endif /* SOLVER_PARALLEL_V5_H */

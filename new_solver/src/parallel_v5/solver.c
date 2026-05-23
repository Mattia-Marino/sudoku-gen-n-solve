#define _POSIX_C_SOURCE 200112L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../include/debug.h"
#include "../../include/solver_parallel_v5.h"
#include "../../include/bitarray.h"

#define SUBDIMENSION	(3)
#define MIN_NUM		(1)
#define MAX_NUM		(9)
#define TOTAL_NUMS	(9)
#define ARRAY_SIZE	(MIN_NUM + TOTAL_NUMS)

static const int MULTIPOTENT_CANDIDATES_V5 =
	1 << (MAX_NUM - 1) | ((1 << (MAX_NUM - 1)) - 1);

/* ------------------------------------------------------------------
 * Sense-reversing spin barrier
 * ------------------------------------------------------------------ */

void spin_barrier_init(struct spin_barrier *b, int n)
{
	b->counter = 0;
	b->sense = 0;
	b->n = n;
}

void spin_barrier_wait(struct spin_barrier *b, int *local_sense)
{
	int new_sense = !*local_sense;
	int new_count;

	*local_sense = new_sense;

	new_count = __atomic_add_fetch(&b->counter, 1, __ATOMIC_SEQ_CST);
	if (new_count == b->n) {
		__atomic_store_n(&b->counter, 0, __ATOMIC_SEQ_CST);
		__atomic_store_n(&b->sense, new_sense, __ATOMIC_SEQ_CST);
	} else {
		while (__atomic_load_n(&b->sense, __ATOMIC_SEQ_CST) != new_sense) {
#if defined(__x86_64__) || defined(__i386__)
			__builtin_ia32_pause();
#endif
		}
	}
}

/* ------------------------------------------------------------------
 * Board lifecycle. Each threadgroup allocates one board and reuses
 * it for every puzzle in its range. reset_and_populate wipes the
 * cell state and loads the new grid in a single pass.
 * ------------------------------------------------------------------ */

int square(int row, int col)
{
	return (((row - 1) / SUBDIMENSION) * SUBDIMENSION) +
		((col - 1) / SUBDIMENSION);
}

void revert_square(int sqn, struct cords indexes[])
{
	struct cords lower_cord = {
		(SUBDIMENSION * ((sqn % SUBDIMENSION) + 1)) - SUBDIMENSION,
		(sqn / SUBDIMENSION) * SUBDIMENSION
	};
	struct cords upper_cord = {
		SUBDIMENSION * ((sqn % SUBDIMENSION) + 1),
		SUBDIMENSION + (SUBDIMENSION * (sqn / SUBDIMENSION))
	};

	indexes[0] = lower_cord;
	indexes[1] = upper_cord;
}

struct board *init_board(struct spin_barrier *barrier)
{
	int i, j;
	struct board *b;

	b = (struct board *)malloc(sizeof(struct board));
	b->unset_cells = TOTAL_NUMS * TOTAL_NUMS;
	b->cells = (struct cell **)malloc(MAX_NUM * sizeof(struct cell *));
	b->step_barrier = barrier;
	b->has_changed = 1;

	for (i = 0; i < MAX_NUM; ++i) {
		b->cells[i] = (struct cell *)malloc(MAX_NUM * sizeof(struct cell));
		for (j = 0; j < MAX_NUM; ++j) {
			b->cells[i][j].has_value = 0;
			b->cells[i][j].value = 0;
			b->cells[i][j].candidates = MULTIPOTENT_CANDIDATES_V5;
		}
	}

	return b;
}

void reset_and_populate(struct board *b, int **grid)
{
	int i, j;
	int unset = TOTAL_NUMS * TOTAL_NUMS;

	for (i = 0; i < MAX_NUM; ++i) {
		for (j = 0; j < MAX_NUM; ++j) {
			if (grid[i][j] != 0) {
				b->cells[i][j].candidates =
					set(0, grid[i][j] - 1);
				b->cells[i][j].value = grid[i][j];
				b->cells[i][j].has_value = 1;
				unset--;
			} else {
				b->cells[i][j].candidates =
					MULTIPOTENT_CANDIDATES_V5;
				b->cells[i][j].value = 0;
				b->cells[i][j].has_value = 0;
			}
		}
	}

	/*
	 * unset_cells and has_changed are read with atomic loads inside
	 * the solver; their initial writes here must also be atomic so
	 * accesses to these locations never mix atomic and non-atomic
	 * operations (which would be UB in C11).
	 */
	__atomic_store_n(&b->unset_cells, unset, __ATOMIC_RELAXED);
	__atomic_store_n(&b->has_changed, 1, __ATOMIC_RELAXED);
}

void board_to_grid(const struct board *b, int **grid)
{
	int i, j;

	for (i = 0; i < MAX_NUM; ++i)
		for (j = 0; j < MAX_NUM; ++j)
			grid[i][j] = b->cells[i][j].value;
}

void free_board(struct board *b)
{
	int i;

	for (i = 0; i < MAX_NUM; ++i)
		free(b->cells[i]);

	free(b->cells);
	free(b);
}

/* ------------------------------------------------------------------
 * Solver core (mirrors serial_v3 logic split across threads)
 * ------------------------------------------------------------------ */

static void __apply_nh_masks(struct cell *cells, struct naked_masks nm)
{
	int mask = nm.n_pair;
	int h_pair = nm.h_pair;
	int h_single = nm.h_single;

	for (int i = 0; i < MAX_NUM; ++i) {
		if (cells[i].has_value)
			continue;
		if (popcount(cells[i].candidates) < 2)
			continue;
		if ((cells[i].candidates & h_pair) != 0)
			cells[i].candidates &= h_pair;
		if ((cells[i].candidates & h_single) != 0)
			cells[i].candidates &= h_single;
		if ((cells[i].candidates & mask) != 0)
			cells[i].candidates &= mask;
	}
}

static struct naked_masks naked_investigator(struct cell *cells)
{
	struct cell r_cell;
	int mask = 0;
	int nc = 0;
	int pairs_array[MAX_NUM];
	int pairs_size = 0;
	int m_once = 0;
	int odds = 0;
	int once = 0;
	int h_pairs = 0;
	int h_pairs_count = 0;
	int bit_count = 0;
	int set_1 = 0, set_2 = 0, set_3 = 0;

	for (int i = 0; i < MAX_NUM; ++i) {
		r_cell = cells[i];
		bit_count = popcount(r_cell.candidates);

		if (r_cell.has_value)
			continue;

		set_3 |= (r_cell.candidates & set_2);
		set_2 |= (r_cell.candidates & set_1);
		set_1 |= (r_cell.candidates);

		m_once |= (odds & r_cell.candidates);
		odds ^= r_cell.candidates;

		if (nc)
			continue;
		if (bit_count != 2)
			continue;

		for (int j = 0; j < pairs_size; ++j) {
			if (pairs_array[j] == r_cell.candidates) {
				nc = 1;
				mask = r_cell.candidates;
				break;
			}
		}
		pairs_array[pairs_size] = r_cell.candidates;
		pairs_size++;
	}

	mask = ~mask;
	once = (odds & (~set_2));
	h_pairs = (set_2 & ~set_3);
	struct naked_masks nm = { once, h_pairs, mask };

	if (popcount(h_pairs) < 2) {
		nm.h_pair = 0;
		return nm;
	}

	for (int i = 0; i < MAX_NUM && h_pairs_count < 2; ++i) {
		if ((h_pairs & cells[i].candidates) == h_pairs)
			h_pairs_count++;
	}
	if (h_pairs_count < 2)
		nm.h_pair = 0;

	return nm;
}

static void apply_nh_masks(struct cell *cell, struct naked_masks masks)
{
	int mask = masks.n_pair;
	int h_pair = masks.h_pair;
	int h_single = masks.h_single;

	if (cell->has_value)
		return;
	if (popcount(cell->candidates) < 2)
		return;

	if ((cell->candidates & h_pair) != 0)
		cell->candidates &= h_pair;
	if ((cell->candidates & h_single) != 0)
		cell->candidates &= h_single;
	if ((cell->candidates & mask) != 0)
		cell->candidates &= mask;
}

static void investigate_naked_pair_row(struct board *b, int r)
{
	struct cell *row = b->cells[r];
	struct naked_masks masks = naked_investigator(row);
	__apply_nh_masks(row, masks);
}

static void investigate_naked_pair_col(struct board *b, int c)
{
	struct cell *cell;
	struct cell r_cell;
	int mask = 0;
	int nc = 0;
	int pairs_array[MAX_NUM];
	int pairs_size = 0;
	int m_once = 0;
	int odds = 0;
	int once = 0;
	int h_pairs = 0;
	int h_pairs_count = 0;
	int bit_count = 0;
	int set_1 = 0, set_2 = 0, set_3 = 0;

	for (int i = 0; i < MAX_NUM; ++i) {
		r_cell = b->cells[i][c];
		bit_count = popcount(r_cell.candidates);

		if (r_cell.has_value)
			continue;

		set_3 |= (r_cell.candidates & set_2);
		set_2 |= (r_cell.candidates & set_1);
		set_1 |= (r_cell.candidates);

		m_once |= (odds & r_cell.candidates);
		odds ^= r_cell.candidates;

		if (nc)
			continue;
		if (bit_count != 2)
			continue;

		for (int j = 0; j < pairs_size; ++j) {
			if (pairs_array[j] == r_cell.candidates) {
				nc = 1;
				mask = r_cell.candidates;
				break;
			}
		}
		pairs_array[pairs_size] = r_cell.candidates;
		pairs_size++;
	}

	mask = ~mask;
	once = (odds & (~set_2));
	h_pairs = (set_2 & ~set_3);

	struct naked_masks nm = { once, h_pairs, mask };

	if (popcount(h_pairs) < 2)
		nm.h_pair = 0;

	for (int i = 0; i < MAX_NUM && h_pairs_count < 2; ++i) {
		if ((h_pairs & b->cells[i][c].candidates) == h_pairs)
			h_pairs_count++;
	}
	if (h_pairs_count < 2)
		nm.h_pair = 0;

	for (int i = 0; i < MAX_NUM; ++i) {
		cell = b->cells[i] + c;
		apply_nh_masks(cell, nm);
	}
}

static void investigate_naked_pair_box(struct board *b, int sqn)
{
	struct cell *cell;
	struct cell r_cell;
	struct cords indexes[2];
	int mask = 0;
	int nc = 0;
	int pairs_array[MAX_NUM];
	int pairs_size = 0;
	int m_once = 0;
	int odds = 0;
	int once = 0;
	int h_pairs = 0;
	int h_pairs_count = 0;
	int bit_count = 0;
	int set_1 = 0, set_2 = 0, set_3 = 0;

	revert_square(sqn, indexes);

	for (int i = indexes[0].x; i < indexes[1].x; ++i) {
		for (int j = indexes[0].y; j < indexes[1].y; ++j) {
			r_cell = b->cells[i][j];
			bit_count = popcount(r_cell.candidates);

			if (r_cell.has_value)
				continue;

			set_3 |= (r_cell.candidates & set_2);
			set_2 |= (r_cell.candidates & set_1);
			set_1 |= (r_cell.candidates);

			m_once |= (odds & r_cell.candidates);
			odds ^= r_cell.candidates;

			if (nc)
				continue;
			if (bit_count != 2)
				continue;

			for (int k = 0; k < pairs_size; ++k) {
				if (pairs_array[k] == r_cell.candidates) {
					nc = 1;
					mask = r_cell.candidates;
					break;
				}
			}
			pairs_array[pairs_size] = r_cell.candidates;
			pairs_size++;
		}
	}

	mask = ~mask;
	once = (odds & (~set_2));
	h_pairs = (set_2 & ~set_3);

	struct naked_masks nm = { once, h_pairs, mask };

	if (popcount(h_pairs) < 2)
		nm.h_pair = 0;

	for (int i = indexes[0].x; i < indexes[1].x; ++i) {
		for (int j = indexes[0].y; j < indexes[1].y; ++j) {
			if ((h_pairs & b->cells[i][j].candidates) == h_pairs)
				h_pairs_count++;
		}
	}
	if (h_pairs_count < 2)
		nm.h_pair = 0;

	for (int i = indexes[0].x; i < indexes[1].x; ++i) {
		for (int j = indexes[0].y; j < indexes[1].y; ++j) {
			cell = b->cells[i] + j;
			apply_nh_masks(cell, nm);
		}
	}
}

static int investigate_row(struct board *b, int r)
{
	struct cell r_cell;
	int mask = 0;

	for (int i = 0; i < MAX_NUM; ++i) {
		r_cell = b->cells[r][i];
		if (r_cell.has_value)
			mask = set(mask, r_cell.value - 1);
	}

	if ((mask ^ MULTIPOTENT_CANDIDATES_V5) == 0)
		return 0;

	mask = ~mask;
	for (int i = 0; i < MAX_NUM; ++i) {
		if (!b->cells[r][i].has_value)
			b->cells[r][i].candidates &= mask;
	}
	return 1;
}

static int investigate_col(struct board *b, int c)
{
	struct cell r_cell;
	int mask = 0;

	for (int i = 0; i < MAX_NUM; ++i) {
		r_cell = b->cells[i][c];
		if (r_cell.has_value)
			mask = set(mask, r_cell.value - 1);
	}

	if ((mask ^ MULTIPOTENT_CANDIDATES_V5) == 0)
		return 0;

	mask = ~mask;
	for (int i = 0; i < MAX_NUM; ++i) {
		if (!b->cells[i][c].has_value)
			b->cells[i][c].candidates &= mask;
	}
	return 1;
}

static int investigate_square(struct board *b, int square_n)
{
	struct cords indexes[2];
	struct cell r_cell;
	int mask = 0;

	revert_square(square_n, indexes);

	for (int i = indexes[0].x; i < indexes[1].x; ++i) {
		for (int j = indexes[0].y; j < indexes[1].y; ++j) {
			r_cell = b->cells[i][j];
			if (r_cell.has_value)
				mask = set(mask, r_cell.value - 1);
		}
	}

	if ((mask ^ MULTIPOTENT_CANDIDATES_V5) == 0)
		return 0;

	mask = ~mask;
	for (int i = indexes[0].x; i < indexes[1].x; ++i) {
		for (int j = indexes[0].y; j < indexes[1].y; ++j) {
			if (!b->cells[i][j].has_value)
				b->cells[i][j].candidates &= mask;
		}
	}
	return 1;
}

/*
 * Cell-by-cell freeze. unset_cells is decremented with an atomic
 * sub so the final count is exact even when multiple threads
 * freeze cells concurrently (each thread owns a disjoint row
 * slice, so the only contention is on the counter itself).
 */
static int freeze_cell_state(struct board *b, struct cell *cell)
{
	int bit_count = popcount(cell->candidates);

	if (!cell->has_value && bit_count == 0)
		return 0;

	if (!cell->has_value && bit_count == 1) {
		cell->value = outer_bit_pos(cell->candidates) + 1;
		cell->has_value = 1;
		__sync_fetch_and_sub(&b->unset_cells, 1);
		return 1;
	}
	return 0;
}

/*
 * Per-thread solver. Each thread handles a contiguous slice of
 * rows/cols/boxes determined by thread_id (1-based) and tg_size.
 * Four barriers per convergence iteration (row/col/box/freeze) --
 * spin-barriers keep the per-wait cost in the ~100 ns range.
 */
void solve_board_thread(struct board *b, int thread_id, int tg_size,
			int *local_sense)
{
	int i, j;
	int local_changed = 0;
	int start = (thread_id - 1) * MAX_NUM / tg_size;
	int end = thread_id * MAX_NUM / tg_size;

	/*
	 * has_changed is read and written by every thread. Use atomic
	 * accesses uniformly so the compiler cannot assume the value
	 * is stable across the loop body (mixing atomic and non-atomic
	 * accesses to the same location is UB in C11 and was causing
	 * non-deterministic mis-solves when GCC -O3 hoisted the plain
	 * read out of the while condition).
	 */
	while (__atomic_load_n(&b->has_changed, __ATOMIC_ACQUIRE)) {
		/*
		 * Match serial_v3 exactly: pass 1 is all row/col/square
		 * value-elimination, pass 2 is all naked/hidden-pair
		 * propagation. Each phase partitions work disjointly by
		 * index; barriers between phases publish the writes.
		 */
		for (i = start; i < end; ++i)
			investigate_row(b, i);
		spin_barrier_wait(b->step_barrier, local_sense);

		for (i = start; i < end; ++i)
			investigate_col(b, i);
		spin_barrier_wait(b->step_barrier, local_sense);

		for (i = start; i < end; ++i)
			investigate_square(b, i);
		spin_barrier_wait(b->step_barrier, local_sense);

		for (i = start; i < end; ++i)
			investigate_naked_pair_row(b, i);
		spin_barrier_wait(b->step_barrier, local_sense);

		for (i = start; i < end; ++i)
			investigate_naked_pair_col(b, i);
		spin_barrier_wait(b->step_barrier, local_sense);

		for (i = start; i < end; ++i)
			investigate_naked_pair_box(b, i);

		/*
		 * Thread 1 resets has_changed before the freeze reduction.
		 * The next barrier guarantees the reset is visible before
		 * any thread OR-writes its local change flag back in.
		 */
		if (thread_id == 1)
			__atomic_store_n(&b->has_changed, 0,
					 __ATOMIC_RELAXED);
		spin_barrier_wait(b->step_barrier, local_sense);

		for (i = start; i < end; ++i)
			for (j = 0; j < MAX_NUM; ++j)
				local_changed |=
					freeze_cell_state(b, &b->cells[i][j]);

		if (local_changed)
			__atomic_fetch_or(&b->has_changed, 1,
					  __ATOMIC_RELAXED);
		local_changed = 0;

		spin_barrier_wait(b->step_barrier, local_sense);
	}

	/*
	 * Exit barrier. Without this, a "fast" thread can read
	 * has_changed==0 and return from solve_board_thread while
	 * other threads are still inside the loop-condition atomic
	 * load; the producer then races ahead to free/reset the board
	 * while a worker is still touching it. The extra barrier
	 * keeps the function call boundary collective.
	 */
	spin_barrier_wait(b->step_barrier, local_sense);
}

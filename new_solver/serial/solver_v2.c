#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "bitarray.h"

#define SUBDIMENSION	(3)
#define MIN_NUM		(1)
#define MAX_NUM		(9)
#define TOTAL_NUMS	(9)
#define ARRAY_SIZE	(MIN_NUM + TOTAL_NUMS)

/*
 * A candidates array represents which values have already been used for a row,
 * column or square.
 */
// Multipotent bitarray definition
//
const int MULTIPOTENT_CANDIDATES = 1 << (MAX_NUM - 1) | ((1<< (MAX_NUM - 1)) - 1 )
typedef int candidates;

/*
 *
 * CELLS AND BOARDS.
 *
 */

/*
 * A cell has a flag to indicate if its value has been set or not, the cell
 * value and three pointers to candidate arrays. One for the row it belongs to,
 * one for the column it belongs to and one for the square it belongs to.
 */
struct cell {
	int has_value;
	int value;
	candidates cs;
};

struct cords{
    int x;
    int y;
};

/*
 * A board has a number of unset cells, a matrix of cells and the candidate
 * arrays for each row, column and square in the board.
 */
struct board {
	int unset_cells;
	struct cell cells[ARRAY_SIZE][ARRAY_SIZE];
}
/*
 * CANDIDATES.
 */


/*
 * All candidates start as multipotent.
 */

void init_candidates(candidates *c)
{
        *c = MULTIPOTENT_CANDIDATES;
}

/*
 * Using a candidate number means marking it as used in the array.
 */

void use_candidate(candidates *cp, int num)
{
    *cp = set(*cp,num);
}

/*
 * Restoring a candidate means marking it as unused in the array.
 */

void restore_candidate(candidates *cp, int num)
{
    if (test_bit(*cp,num))
        *cp = invert_bit(*cp,num);
}

/*
 * Auxiliar. Calculates the square number for the given cell. Squares are
 * numberd from top to bottom, left to right.
 */
int square(int row, int col)
{
	return (((row - 1) / SUBDIMENSION) * SUBDIMENSION) +
		((col - 1) / SUBDIMENSION);
}

// Sqaures from 0 to 8 counting from top left to bottom right
void revert_square(int sqn,struct cords indexes[])
{
    // lower included --> upper excluded
    int x_lower =  
    int x_upper = SUBDIMENSION * ((sqn % SUBDIMENSION) + 1)
        
    int y_lower =  
    int y_upper = (sqn/(SUBDIMENSION)) * (SUBDIMENSION - (sqn%SUBDIMENSION) + 1)

    struct cord lower_cord = {
        (SUBDIMENSION * ((sqn % SUBDIMENSION )+1)) - SUBDIMENSION,
        (sqn/(SUBDIMENSION)) * SUBDIMENSION - 1
    }

    struct cord upper_cord = {
        SUBDIMENSION * ((sqn % SUBDIMENSION) + 1),
        (sqn/(SUBDIMENSION)) * (SUBDIMENSION - (sqn%SUBDIMENSION) + 1)
    }

    indexes[0] = lower_cord;
    indexes[1] = upper_cord;

    return;


}

/*
 * Every board starts empty. Cell candidate pointers are established.
 */
void init_board(struct board *b)
{
	int i;
	int j;
	b->unset_cells = TOTAL_NUMS * TOTAL_NUMS;
	for (i = MIN_NUM; i <= MAX_NUM; ++i) {
		for (j = MIN_NUM; j <= MAX_NUM; ++j) {
			b->cells[i][j].has_value = 0;
			b->cells[i][j].value = 0;
			b->cells[i][j].cs = MULTIPOTENT_CANDIDATES;
		}
	}
}

/*
 * Sets a cell value in the given board.
 */
void set_cell(struct board *b, int r, int c, int val)
{
	b->unset_cells -= 1;
	b->cells[r][c].has_value = 1;
	b->cells[r][c].value = val;
	use_candidate(&(b->cells[r][c].cs), val);
}

/*
 * Unsets a cell value in the given board.
 */
void unset_cell(struct board *b, int r, int c, int val)
{
	b->unset_cells += 1;
	b->cells[r][c].has_value = 0;
	b->cells[r][c].value = 0;
	restore_candidate(&(b->cells[r][c].cs), val);
}

/*
 * Checks if a cell value is set. Returns 1 if set, 0 otherwise.
 */
int is_set(struct board *b, int r, int c)
{
	return (b->cells[r][c].has_value);
}

/*
 * Calculates the number following a given one circularly.
 */
int following(int num)
{
	return ((num - MIN_NUM + 1) % TOTAL_NUMS + MIN_NUM);
}

/*
 * Calculates the cell following a given one. Advances from top to bottom and
 * left to right. Returns 0 if there is no next cell, 1 otherwise and modifies
 * the arguments to point to the next cell in that case.
 */
int next_cell(int *r, int *c)
{
	if ((*r) == MAX_NUM && (*c) == MAX_NUM)
		return 0;
	*c = following(*c);
	if ((*c) == MIN_NUM)
		(*r) = following(*r);
	return 1;
}

/*
 * Prints the given board on screen.
 */
void print_board(struct board *b)
{
	int i;
	int j;

	assert_(b != NULL);

	for (i = MIN_NUM; i <= MAX_NUM; ++i) {
		for (j = MIN_NUM; j <= MAX_NUM; ++j)
			printf(" %d", b->cells[i][j].value);
		printf("\n");
	}
}

void read_board(FILE *f, struct board *b)
{
	int row;
	int col;
	int c;

	assert_(f != NULL && b != NULL);

	row = MIN_NUM;
	col = MIN_NUM;

	while(1) {
		c = fgetc(f);
		if ((isdigit(c) && c != '0') || c == '.') {
			if (c != '.')
				set_cell(b, row, col, (c - '0'));
			if (! next_cell(&row, &col))
				break;
		}
	}
}


/*
 * TODO: Factorize by extracting row, column and box iterators
 * TODO: Use iterator to investigate row, columns and boxes in each investigation step
 * TODO: Factorize and extract mask applying function as it is common to all investigation functions
 * TODO: Naked pairs investigation will be implemented as follows --> Create a multipotent mask, use this mask in & operation with all cell-associated mask (same row,col,box), if the mask stays the same at least 2 times then a naked pair is found
 */
void incestigate_naked_pair(struct board *b)
{

    // investigate row column and boxes for naked pairs and apply mask
    return;
}

int investigate_row(struct board *b,int r)
{
    struct cell row[] = b->cells[r];
    struct cell r_cell;
    int mask = 0;
    for (int i=0;i<MAX_NUM;++i)
    {
        r_cell = row[i];
        if (r_cell.has_value)
            mask = set(mask,r_cell.value);
    }

    if (mask ^ MULTIPOTENT_CANDIDATES == 0)
        return 0;

    for (int i=0;i<MAX_NUM;++i)
    {
        r_cell = row[i];
        if (! r_cell.has_value)
            r_cell.cs &= mask;
    }
    return 1;
}


int investigate_col(struct board *b,int c)
{
    struct cell r_cell;
    int mask = 0;
    int changed = 1;
    for (int i=0;i<MAX_NUM;++i)
    {
        r_cell = b->cells[i][c];
        if (r_cell.has_value)
            mask = set(mask,r_cell.value);
    }

    if (mask ^ MULTIPOTENT_CANDIDATES == 0)
        return 0;

    for (int i=0;i<MAX_NUM;++i)
    {
        r_cell = b->cells[i][c];
        if (! r_cell.has_value)
            r_cell.cs &= mask;
    }

    return 1;
}

int investigate_square(struct board *b,int square_n)
{
    struct cord indexes[2];
    struct cell r_cell;
    int mask = 0;

    revert_square(square_n,indexes);
    
    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            r_cell = col[i][j];
            if (r_cell.has_value)
                mask = set(mask,r_cell.value);
        }
    }

    if (mask ^ MULTIPOTENT_CANDIDATES == 0)
        return 0;

    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            r_cell = col[i][j];
            if (! r_cell.has_value)
                r_cell.cs &= mask;
        }
    }

    return 1;
}

//function need to implement tie breaker algorithm when more than a single value is a candidate for this cell 

int solve_multi_value(int cs)
{
    return 1;
}

int freeze_cell_state(struct board *b)
{
    int has_changed = 0;
    //for (int bit_toleration = 1;bit_toleration<3;++bit_toleration){
    for (int i=0;i<MAX_NUM;++i){
        for (int j=0;j<MAX_NUM;++j){
            struct cell = b->cells[i][j];
            int bit_count = popcount(cell.cs)
                if((!cell.has_value) && bit_count == 0){
                    return 0;
                }else if ((!cell.has_value)  && bit_count == 1){
                    cell.value = outer_bit_pos(cs);
                    cell.has_value = 1;
                    has_changed = 1;
                    b->unset_cell--;
                }
            /*
             * else if ((!cell.has_value) && bit_count <=bit_toleration){
             *   cell.value = solve_multi_value(cell.cs);
             *   cell.has_value = 1;
             *   has_changed = 1;
             *   b->unset_cell--;
             *   }
             */

        }
    }
    //}

    return has_changed;
}

/*
 * Solves a board starting with the given cell. Returns 1 if the board could be
 * solved, 0 if not.
 */
int solve_board(struct board *b, int r, int c)
{
    int has_changed  = 1;
    while (has_changed)
    {
        for (int i=0;i<MAX_NUM;++i){
            investigate_row(b,i);
            investigate_col(b,i);
            investigate_square(b,i);
            // Check for naked candidates
            naked_pair_investigation(b)
        }

        has_changed = freeze_cell_state(b);
    }
    return b->unset_cells == 0;
}

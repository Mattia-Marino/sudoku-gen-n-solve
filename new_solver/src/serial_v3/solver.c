#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../include/debug.h"
#include "../../include/solver_v3.h"
#include "../../include/bitarray.h"

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
const int MULTIPOTENT_CANDIDATES = 1 << (MAX_NUM - 1) | ((1<< (MAX_NUM - 1)) - 1 );

/*
 * Auxiliar. Calculates the square number for the given cell. Squares are
 * numberd from top to bottom, left to right.
 */
int square(int row, int col)
{
	return (((row - 1) / SUBDIMENSION) * SUBDIMENSION) +
		((col - 1) / SUBDIMENSION);
}

// Squares from 0 to 8 counting from top left to bottom right
void revert_square(int sqn,struct cords indexes[])
{
    // lower included --> upper excluded
        
    struct cords lower_cord = {(SUBDIMENSION * ((sqn % SUBDIMENSION )+1)) - SUBDIMENSION, (sqn/(SUBDIMENSION)) * SUBDIMENSION};

    struct cords upper_cord = {SUBDIMENSION * ((sqn % SUBDIMENSION) + 1) , SUBDIMENSION  + (SUBDIMENSION * (sqn/SUBDIMENSION) )};

    indexes[0] = lower_cord;
    indexes[1] = upper_cord;

    return;
}

/*
 * Every board starts empty. Cell candidate pointers are established.
 */
//DONE
struct board *init_board()
{
	int i,j;
	struct board *b;

	b = (struct board *) malloc(sizeof(struct board));
	b->unset_cells = TOTAL_NUMS * TOTAL_NUMS;
	b->cells = (struct cell **) malloc(MAX_NUM * sizeof(struct cell*));

	for (i = 0; i < MAX_NUM; ++i) {
		b->cells[i] = (struct cell *) malloc(MAX_NUM * sizeof(struct cell));
		for (j = 0; j < MAX_NUM; ++j) {
			b->cells[i][j].has_value = 0;
			b->cells[i][j].value = 0;
			b->cells[i][j].candidates = MULTIPOTENT_CANDIDATES;
		}
	}

    return b;
}

//DONE
void populate_board(struct board *b,int **grid)
{
    int i,j;

	for (i = 0; i < MAX_NUM; ++i) {
		for (j = 0; j < MAX_NUM; ++j) {
			if (grid[i][j] != 0) {
				b->cells[i][j].candidates = set(0, grid[i][j]-1);
                b->cells[i][j].value = grid[i][j];
                b->cells[i][j].has_value = 1;
			}
		}
	}
}


/*
 * Prints the given board on screen.
 * DONE
 */
void print_board(struct board *b)
{
	int i;
	int j;


	for (i = 0; i < MAX_NUM; ++i) {
		for (j = 0; j < MAX_NUM; ++j)
        {
			printf(" %d", b->cells[i][j].value);
			if (j < MAX_NUM - 1)
				printf(" ");
        }
		printf("\n");
	}
}

//DONE
void board_to_grid(const struct board *b, int **grid)
{
	int i, j;
	for (i = 0; i < MAX_NUM; i++) {
		for (j = 0; j < MAX_NUM; j++) {
            grid[i][j] = b->cells[i][j].value;
		}
	}

}

// DONE
void free_board(struct board *b)
{
	int i;

	for (i = 0; i < MAX_NUM; i++)
		free(b->cells[i]);

	free(b->cells);
	free(b);
}

void apply_nh_masks(struct cell *cells, struct naked_masks nm){
    int mask = nm.n_pair;
    int h_pair = nm.h_pair;
    int h_single = nm.h_single;

    DPRINTF("Retrieved hidden single mask %d , hidden pair mask %d and naked pair mask %d\n",h_single,h_pair,mask);
    for (int i=0;i<MAX_NUM;++i){
        if (cells[i].has_value)
            continue;

        if (popcount(cells[i].candidates) < 2)
            continue;

        // Resolve hidden pairs
        if ((cells[i].candidates & h_pair) != 0){
            DPRINTF("Applying hidden pair mask to candidates %d\n",cells[i].candidates);
            cells[i].candidates &= h_pair;
        }
        // Resolve hidden singles
        if ((cells[i].candidates & h_single) != 0){
            DPRINTF("Applying hidden single mask to candidates %d\n",cells[i].candidates);
            cells[i].candidates &= h_single;
        }

        //Resolve naked pairs
        if ((cells[i].candidates & mask) != 0){
            DPRINTF("Old cell [%d] mask %d\n",i,cells[i].candidates);
            cells[i].candidates &= mask;
            DPRINTF("New cell [%d] mask %d\n",i,cells[i].candidates);
        }
    }
    return;

}

struct naked_masks naked_investigator(struct cell *cells){
    struct cell r_cell;
    int mask = 0;
    //naked candidate found flag
    int nc = 0;
    int pairs_array[MAX_NUM];
    int pairs_size = 0;
    // more than once mask
    int m_once = 0;
    int odds = 0;
    int once = 0;
    // hidden sets masks
    int h_pairs = 0;
    int h_pairs_count = 0;
    int bit_count = 0;

    int set_1 = 0;
    int set_2 = 0;
    int set_3 = 0;

    for (int i=0;i<MAX_NUM;++i)
    {
        r_cell = cells[i];
        bit_count = popcount(r_cell.candidates);

        if (r_cell.has_value)
            continue;

        set_3 |= (r_cell.candidates & set_2);
        set_2 |= (r_cell.candidates & set_1);
        set_1 |= (r_cell.candidates);

        m_once |= (odds & r_cell.candidates);
        odds ^= r_cell.candidates;
        DPRINTF("Current [%d] candidates %d\n",i,r_cell.candidates);

        if (nc)
            continue;

        if (bit_count != 2)
            continue;

        for (int j=0;j<pairs_size;++j){
            if (pairs_array[j] == r_cell.candidates){
                nc = 1;
                mask = r_cell.candidates;
                break;
            }
        }

        pairs_array[pairs_size] = r_cell.candidates;
        pairs_size++;
    }

    mask = ~mask;
    // Find candidates only repeted once (hidden single)
    // m_once &= mask;
    once = (odds & (~set_2));
    // once = popcount(once) != 1 ? MULTIPOTENT_CANDIDATES : once;
    h_pairs = (set_2 & ~set_3);
    struct naked_masks nm = {once,h_pairs,mask};
    if (popcount(h_pairs) < 2){
        nm.h_pair = 0;
        return nm;
    }
    
    for (int i=0;i<MAX_NUM && h_pairs_count < 2;++i)
    {
        if ((h_pairs & cells[i].candidates) == h_pairs)
        {
            h_pairs_count++;
        }
    }

    if ( h_pairs_count < 2)
        nm.h_pair = 0;
    
    return nm;

}

//DONE
void investigate_naked_pair_row(struct board *b,int r)
{

    // investigate row column and boxes for naked pairs and apply mask

    struct cell *row = b->cells[r];
    struct naked_masks masks = naked_investigator(row);
    apply_nh_masks(row,masks);
    return;
}

//DONE
void investigate_naked_pair_col(struct board *b,int c)
{
    struct cell r_cell;
    int mask;
    //naked candidate found flag
    int nc = 0;
    int pairs_array[MAX_NUM];
    int pairs_size = 0;
    for (int i=0;i<MAX_NUM;++i)
    {
        r_cell = b->cells[i][c];

        if (r_cell.has_value || popcount(r_cell.candidates) != 2)
            continue;

        for (int j=0;j<pairs_size;++j){
            if (pairs_array[j] == r_cell.candidates){
                nc = 1;
                mask = r_cell.candidates;
                break;
            }
        }
        if (nc)
            break;

        pairs_array[pairs_size] = r_cell.candidates;
        pairs_size++;
    }

    if (! nc)
        return;
    DPRINTF("Calculated column naked pair mask %d\n",mask);
    //Invert mask for merge
    mask = ~mask;

    for (int i=0;i<MAX_NUM;++i){
        r_cell = b->cells[i][c];
        if (! r_cell.has_value && ((r_cell.candidates & mask) != 0)){
            DPRINTF("Old cell [%d][%d] mask %d\n",i,c,b->cells[i][c].candidates);
            b->cells[i][c].candidates &= mask;
            DPRINTF("New cell [%d][%d] mask %d\n",i,c,b->cells[i][c].candidates);
        }
    }
    // investigate row column and boxes for naked pairs and apply mask
    return;
}

void investigate_naked_pair_box(struct board *b,int sqn)
{
    struct cords indexes[2];
    struct cell r_cell;
    int mask;
    //naked candidate found flag
    int nc = 0;
    int pairs_array[MAX_NUM];
    int pairs_size = 0;
    revert_square(sqn,indexes);

    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            r_cell = b->cells[i][j];
            if (r_cell.has_value || popcount(r_cell.candidates) != 2)
                continue;

            for (int k=0;k<pairs_size;++k){
                if (pairs_array[k] == r_cell.candidates){
                    nc = 1;
                    mask = r_cell.candidates;
                    break;
                }
            }
            if (nc)
                break;

            pairs_array[pairs_size] = r_cell.candidates;
            pairs_size++;
        }
    }

    if (! nc)
        return;


    DPRINTF("Calculated box naked pair mask %d\n",mask);
    //Invert mask for merge
    mask = ~mask;
    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            r_cell = b->cells[i][j];
            if (! r_cell.has_value && ((r_cell.candidates & mask) != 0)){
                DPRINTF("Old cell [%d][%d] mask %d\n",i,j,b->cells[i][j].candidates);
                b->cells[i][j].candidates &= mask;
                DPRINTF("New cell [%d][%d] mask %d\n",i,j,b->cells[i][j].candidates);

            }
        }
    }
    // investigate row column and boxes for naked pairs and apply mask
    return;
}




//DONE
int investigate_row(struct board *b,int r)
{
    struct cell r_cell;
    int mask = 0;
    for (int i=0;i<MAX_NUM;++i)
    {
        r_cell = b->cells[r][i];
        if (r_cell.has_value)
            mask = set(mask,r_cell.value-1);
    }

    DPRINTF("Calculated row mask %d\n",mask);
    if ((mask ^ MULTIPOTENT_CANDIDATES) == 0)
        return 0;
    //Invert mask for merge
    mask = ~mask;

    for (int i=0;i<MAX_NUM;++i)
    {
        if (! b->cells[r][i].has_value){
            DPRINTF("Old cell [%d][%d] mask %d\n",r,i,b->cells[r][i].candidates);
            b->cells[r][i].candidates &= mask;
            DPRINTF("New cell [%d][%d] mask %d\n",r,i,b->cells[r][i].candidates);
        }
    }
    return 1;
}

//DONE
int investigate_col(struct board *b,int c)
{
    struct cell r_cell;
    int mask = 0;
    for (int i=0;i<MAX_NUM;++i)
    {
        r_cell = b->cells[i][c];
        if (r_cell.has_value)
            mask = set(mask,r_cell.value-1);
    }

    DPRINTF("Calculated col mask %d\n",mask);
    if ((mask ^ MULTIPOTENT_CANDIDATES) == 0)
        return 0;

    //Invert mask for merge
    mask = ~mask;
    for (int i=0;i<MAX_NUM;++i)
    {
        if (! b->cells[i][c].has_value){
            DPRINTF("Old cell [%d][%d] mask %d\n",i,c,b->cells[i][c].candidates);
            b->cells[i][c].candidates &= mask;
            DPRINTF("New cell [%d][%d] mask %d\n",i,c,b->cells[i][c].candidates);
        }
    }

    return 1;
}

int investigate_square(struct board *b,int square_n)
{
    struct cords indexes[2];
    struct cell r_cell;
    int mask = 0;

    revert_square(square_n,indexes);
    
    DPRINTF("Checking square %d from col [%d] to [%d] and row [%d] to [%d]\n",square_n,indexes[0].x,indexes[1].x,indexes[0].y,indexes[1].y);
    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            DPRINTF("Checking cell [%d][%d]\n",i,j);
            r_cell = b->cells[i][j];
            if (r_cell.has_value)
                mask = set(mask,r_cell.value-1);
        }
    }

    DPRINTF("Calculated square mask %d\n",mask);
    if ((mask ^ MULTIPOTENT_CANDIDATES) == 0)
        return 0;
    //Invert mask for merge
    mask = ~mask;

    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            if (! b->cells[i][j].has_value)
                b->cells[i][j].candidates &= mask;
        }
    }

    return 1;
}

int freeze_cell_state(struct board *b)
{
    struct cell cell;
    int bit_count;
    int has_changed = 0;
    int val;
    //for (int bit_toleration = 1;bit_toleration<3;++bit_toleration){
    for (int i=0;i<MAX_NUM;++i){
        for (int j=0;j<MAX_NUM;++j){
            cell = b->cells[i][j];
            bit_count = popcount(cell.candidates);
            DPRINTF("Found cell [%d][%d]  with %d candidates\n",i,j,bit_count);
            if((!cell.has_value) && bit_count == 0){
                DPRINTF("Found cell [%d][%d]  with 0 candidates, can't resolve the sudoku\n",i,j);
                return 0;
            }else if ((!cell.has_value) && bit_count == 1){
                val = outer_bit_pos(cell.candidates) + 1;
                DPRINTF("Found cell [%d][%d]  with 1 candidates, setting value %d\n",i,j,val);
                b->cells[i][j].value = val;
                b->cells[i][j].has_value = 1;
                has_changed = 1;
                b->unset_cells--;
            }
        }
    }
    //}

    return has_changed;
}

/*
 * Solves a board starting with the given cell. Returns 1 if the board could be
 * solved, 0 if not.
 */
int solve_board(struct board *b)
{
    int has_changed  = 1;
    while (has_changed)
    {
        for (int i=0;i<MAX_NUM;++i){
            investigate_row(b,i);
            investigate_col(b,i);
            investigate_square(b,i);
            // Check for naked candidates
        }
        for (int i=0;i<MAX_NUM;++i){
            investigate_naked_pair_row(b,i);
            investigate_naked_pair_col(b,i);
            investigate_naked_pair_box(b,i);
            // Check for naked candidates
        }

        has_changed = freeze_cell_state(b);
        DPRINTF("New board state\n");
        DPRINT_BOARD(b);
    }
    return b->unset_cells == 0;
}

int sudoku_solver(int **grid, int n)
{
	int is_solved; /* Flag to check if is solved*/
	struct board *b;

	/* Create an extended grid */
	b = init_board();
	populate_board(b, grid);
	DPRINT_BOARD(b);


    is_solved = solve_board(b);

    if (! is_solved){
        printf("Sudoku not solved\n");
        DPRINT_BOARD(b);
    }
	/* Convert in the original grid */
	board_to_grid(b, grid);

	/* Free everything */
	free_board(b);

	return 0;
}

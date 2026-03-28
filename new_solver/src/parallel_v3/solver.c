#define _POSIX_C_SOURCE 200112L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../include/debug.h"
#include "../../include/solver_parallel_v3.h"
#include "../../include/bitarray.h"
// Include rw monitors for cell access
#include "../../include/monitors.h"
#include "../../include/pthread_groups.h"

#define SUBDIMENSION	(3)
#define MIN_NUM		(1)
#define MAX_NUM		(9)
#define TOTAL_NUMS	(9)
#define ARRAY_SIZE	(MIN_NUM + TOTAL_NUMS)
typedef void *( *target )( void *);
const int tg_size = 3;
/*
 * A candidates array represents which values have already been used for a row,
 * column or square.
 */
// Multipotent bitarray definition
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

struct board *init_board()
{
	int i,j;
	struct board *b;
    pthread_barrier_t investigation_barrier;
    pthread_barrier_t freeze_barrier;
    pthread_mutex_t change_lock;
	b = (struct board *) malloc(sizeof(struct board));
	b->unset_cells = TOTAL_NUMS * TOTAL_NUMS;
	b->cells = (struct cell **) malloc(MAX_NUM * sizeof(struct cell*));
    b->counter_monitor = (struct RW_monitor*) malloc(sizeof(struct RW_monitor));
    b->has_changed = 1;
    pthread_barrier_init(&investigation_barrier,NULL,3);
    pthread_barrier_init(&freeze_barrier,NULL,3);
    pthread_mutex_init(&change_lock,NULL);
    b->investigation_barrier = investigation_barrier;
    b->freeze_barrier = freeze_barrier;
    b->change_lock = change_lock;
    init_rw_monitor(b->counter_monitor);

	for (i = 0; i < MAX_NUM; ++i) {
		b->cells[i] = (struct cell *) malloc(MAX_NUM * sizeof(struct cell));
		for (j = 0; j < MAX_NUM; ++j) {
			b->cells[i][j].has_value = 0;
			b->cells[i][j].value = 0;
			b->cells[i][j].candidates = MULTIPOTENT_CANDIDATES;
            // Monitors initialization
			b->cells[i][j].cs_monitor = malloc(sizeof(struct RW_monitor));
            init_rw_monitor(b->cells[i][j].cs_monitor);
			b->cells[i][j].val_monitor = malloc(sizeof(struct RW_monitor));
            init_rw_monitor(b->cells[i][j].val_monitor);
		}
	}

    return b;
}


void populate_board(struct board *b,int **grid)
{
    int i,j;

	for (i = 0; i < MAX_NUM; ++i) {
		for (j = 0; j < MAX_NUM; ++j) {
			if (grid[i][j] != 0) {
				b->cells[i][j].candidates = set(0, grid[i][j]-1);
                b->cells[i][j].value = grid[i][j];
                b->cells[i][j].has_value = 1;
                b->unset_cells--;
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

	for (i = 0; i < MAX_NUM; i++){
        for (int j =0; j<MAX_NUM;++j){
            rw_monitor_destroy(b->cells[i][j].cs_monitor);
            rw_monitor_destroy(b->cells[i][j].val_monitor);
            free(b->cells[i][j].cs_monitor);
            free(b->cells[i][j].val_monitor);
        }
		free(b->cells[i]);
    }

    rw_monitor_destroy(b->counter_monitor);
    pthread_barrier_destroy(&b->freeze_barrier);
    pthread_barrier_destroy(&b->investigation_barrier);
    free(b->counter_monitor);
	free(b->cells);
	free(b);
}

void __apply_nh_masks(struct cell *cells, struct naked_masks nm){
    int mask = nm.n_pair;
    int h_pair = nm.h_pair;
    int h_single = nm.h_single;

    DPRINTF("Retrieved hidden single mask %d , hidden pair mask %d and naked pair mask %d\n",h_single,h_pair,mask);
    for (int i=0;i<MAX_NUM;++i){
        DPRINTF("Write lock acquiring\n");
        rw_monitor_request_write(cells[i].cs_monitor);
        DPRINTF("Write lock acquired\n");
        DPRINTF("Read lock acquiring\n");
        rw_monitor_request_read(cells[i].val_monitor);
        DPRINTF("Read lock acquired\n");

        if (cells[i].has_value){
            rw_monitor_release_write(cells[i].cs_monitor);
            rw_monitor_release_read(cells[i].val_monitor);
            DPRINTF("Write and read lock released\n");
            continue;
        }

        rw_monitor_release_read(cells[i].val_monitor);
        DPRINTF("Read lock released\n");

        if (popcount(cells[i].candidates) < 2)
        {
            rw_monitor_release_write(cells[i].cs_monitor);
            DPRINTF("Write lock released\n");
            continue;
        }

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
        rw_monitor_release_write(cells[i].cs_monitor);
        DPRINTF("Write lock released\n");
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
        DPRINTF("Acquiring cell[%d] read candidates\n",i);
        rw_monitor_request_read(cells[i].cs_monitor);
        DPRINTF("Acquired cell[%d] read candidates\n",i);
        DPRINTF("Acquiring cell[%d] read value\n",i);
        rw_monitor_request_read(cells[i].val_monitor);
        DPRINTF("Acquired cell[%d] read value\n",i);
        r_cell = cells[i];
        bit_count = popcount(r_cell.candidates);

        if (r_cell.has_value){
            DPRINTF("Releasing cell[%d] read candidates and value\n",i);
            rw_monitor_release_read(cells[i].cs_monitor);
            rw_monitor_release_read(cells[i].val_monitor);
            continue;
        }

        DPRINTF("Releasing cell[%d] read value\n",i);
        rw_monitor_release_read(cells[i].val_monitor);
        DPRINTF("Released cell[%d] read value\n",i);

        set_3 |= (r_cell.candidates & set_2);
        set_2 |= (r_cell.candidates & set_1);
        set_1 |= (r_cell.candidates);

        m_once |= (odds & r_cell.candidates);
        odds ^= r_cell.candidates;
        DPRINTF("Current [%d] candidates %d\n",i,r_cell.candidates);

        if (nc){
            DPRINTF("Releasing cell[%d] candidates value\n",i);
            rw_monitor_release_read(cells[i].cs_monitor);
            DPRINTF("Released cell[%d] candidates value\n",i);
            continue;
        }

        if (bit_count != 2){
            DPRINTF("Releasing cell[%d] candidates value\n",i);
            rw_monitor_release_read(cells[i].cs_monitor);
            DPRINTF("Released cell[%d] candidates value\n",i);
            continue;
        }

        for (int j=0;j<pairs_size;++j){
            if (pairs_array[j] == r_cell.candidates){
                nc = 1;
                mask = r_cell.candidates;
                break;
            }
        }

        pairs_array[pairs_size] = r_cell.candidates;
        pairs_size++;
        DPRINTF("Releasing cell[%d] candidates value\n",i);
        rw_monitor_release_read(cells[i].cs_monitor);
        DPRINTF("Released cell[%d] candidates value\n",i);
    }

    mask = ~mask;
    // Find candidates only repeted once (hidden single)
    once = (odds & (~set_2));
    h_pairs = (set_2 & ~set_3);
    struct naked_masks nm = {once,h_pairs,mask};
    if (popcount(h_pairs) < 2){
        nm.h_pair = 0;
        return nm;
    }
    
    for (int i=0;i<MAX_NUM && h_pairs_count < 2;++i)
    {
        rw_monitor_request_read(cells[i].cs_monitor);
        if ((h_pairs & cells[i].candidates) == h_pairs)
        {
            h_pairs_count++;
        }
        rw_monitor_release_read(cells[i].cs_monitor);
    }

    if ( h_pairs_count < 2)
        nm.h_pair = 0;
    
    return nm;

}

void apply_nh_masks(struct cell* cell,struct naked_masks masks){
    int mask = masks.n_pair;
    int h_pair = masks.h_pair;
    int h_single = masks.h_single;

    DPRINTF("[apply_nh_mask] Acquiring value read lock\n");
    rw_monitor_request_read(cell->val_monitor);
    DPRINTF("[apply_nh_mask] Acquired value read lock\n");
    if (cell->has_value){
        rw_monitor_release_read(cell->val_monitor);
        DPRINTF("[apply_nh_mask] Released value read lock\n");
        return;
        }
    rw_monitor_release_read(cell->val_monitor);
    DPRINTF("[apply_nh_mask] Released value read lock\n");
    DPRINTF("[apply_nh_mask] Acquiring candidate read lock\n");
    rw_monitor_request_write(cell->cs_monitor);
    DPRINTF("[apply_nh_mask] Acquired candidate read lock\n");
    if (popcount(cell->candidates) < 2){
        rw_monitor_release_write(cell->cs_monitor);
        DPRINTF("[apply_nh_mask] Released candidate read lock\n");
        return;
    }

    // Resolve hidden pairs
    if ((cell->candidates & h_pair) != 0){
        DPRINTF("[apply_nh_mask] Applying hidden pair mask to candidates %d\n",cell->candidates);
        cell->candidates &= h_pair;
    }
    // Resolve hidden singles
    if ((cell->candidates & h_single) != 0){
        DPRINTF("[apply_nh_mask] Applying hidden single mask to candidates %d\n",cell->candidates);
        cell->candidates &= h_single;
    }

    //Resolve naked pairs
    if ((cell->candidates & mask) != 0){
        DPRINTF("Old cell mask %d\n",cell->candidates);
        cell->candidates &= mask;
        DPRINTF("[apply_nh_mask] New cell mask %d\n",cell->candidates);
    }
    rw_monitor_release_write(cell->cs_monitor);
    DPRINTF("[apply_nh_mask] Released candidate read lock\n");
    return;

}


void investigate_naked_pair_row(struct board *b,int r)
{

    // investigate row column and boxes for naked pairs and apply mask

    struct cell *row = b->cells[r];
    struct naked_masks masks = naked_investigator(row);
    __apply_nh_masks(row,masks);
    return;
}


void investigate_naked_pair_col(struct board *b,int c)
{

    struct cell *cell;
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
        rw_monitor_request_read(b->cells[i][c].cs_monitor);
        rw_monitor_request_read(b->cells[i][c].val_monitor);
        r_cell = b->cells[i][c];
        bit_count = popcount(r_cell.candidates);

        if (r_cell.has_value){
            rw_monitor_release_read(b->cells[i][c].cs_monitor);
            rw_monitor_release_read(b->cells[i][c].val_monitor);
            continue;
        }
        rw_monitor_release_read(b->cells[i][c].val_monitor);

        set_3 |= (r_cell.candidates & set_2);
        set_2 |= (r_cell.candidates & set_1);
        set_1 |= (r_cell.candidates);

        m_once |= (odds & r_cell.candidates);
        odds ^= r_cell.candidates;
        DPRINTF("Current [%d] candidates %d\n",i,r_cell.candidates);

        if (nc){

            rw_monitor_release_read(b->cells[i][c].cs_monitor);
            continue;
        }

        if (bit_count != 2)
        {
            rw_monitor_release_read(b->cells[i][c].cs_monitor);
            continue;
        }

        for (int j=0;j<pairs_size;++j){
            if (pairs_array[j] == r_cell.candidates){
                nc = 1;
                mask = r_cell.candidates;
                break;
            }
        }

        pairs_array[pairs_size] = r_cell.candidates;
        pairs_size++;
        rw_monitor_release_read(b->cells[i][c].cs_monitor);
    }

    mask = ~mask;
    once = (odds & (~set_2));
    h_pairs = (set_2 & ~set_3);

    struct naked_masks nm = {once,h_pairs,mask};

    if (popcount(h_pairs) < 2){
        nm.h_pair = 0;
    }
    
    for (int i=0;i<MAX_NUM && h_pairs_count < 2;++i)
    {
        rw_monitor_request_read(b->cells[i][c].cs_monitor);
        if ((h_pairs & b->cells[i][c].candidates) == h_pairs)
        {
            h_pairs_count++;
        }
        rw_monitor_release_read(b->cells[i][c].cs_monitor);
    }

    if ( h_pairs_count < 2)
        nm.h_pair = 0;
    

    for (int i=0;i<MAX_NUM;++i){
        cell = b->cells[i] + c;
        apply_nh_masks(cell,nm);
    }
    // investigate row column and boxes for naked pairs and apply mask
    return;
}

void investigate_naked_pair_box(struct board *b,int sqn)
{
    struct cell *cell;
    struct cell r_cell;
    struct cords indexes[2];
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
    revert_square(sqn,indexes);

    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            DPRINTF("[naked box] Acquiring read candidate and value lock\n");
            rw_monitor_request_read(b->cells[i][j].cs_monitor);
            rw_monitor_request_read(b->cells[i][j].val_monitor);
            DPRINTF("[naked box] Acquired read candidate and value lock\n");
            r_cell = b->cells[i][j];
            bit_count = popcount(r_cell.candidates);

            if (r_cell.has_value){
                rw_monitor_release_read(b->cells[i][j].cs_monitor);
                rw_monitor_release_read(b->cells[i][j].val_monitor);
            DPRINTF("[naked box] Released read candidate and value lock\n");
                continue;
            }
            rw_monitor_release_read(b->cells[i][j].val_monitor);
            DPRINTF("[naked box] Released read value lock\n");

            set_3 |= (r_cell.candidates & set_2);
            set_2 |= (r_cell.candidates & set_1);
            set_1 |= (r_cell.candidates);

            m_once |= (odds & r_cell.candidates);
            odds ^= r_cell.candidates;
            DPRINTF("[naked box] Current [%d] candidates %d\n",i,r_cell.candidates);

            if (nc){
                rw_monitor_release_read(b->cells[i][j].cs_monitor);
                DPRINTF("[naked box] Released read candidate lock\n");
                continue;
            }

            if (bit_count != 2){
                rw_monitor_release_read(b->cells[i][j].cs_monitor);
                DPRINTF("[naked box] Released read candidate lock\n");
                continue;
            }

            for (int j=0;j<pairs_size;++j){
                if (pairs_array[j] == r_cell.candidates){
                    nc = 1;
                    mask = r_cell.candidates;
                    break;
                }
            }

            pairs_array[pairs_size] = r_cell.candidates;
            pairs_size++;
            rw_monitor_release_read(b->cells[i][j].cs_monitor);
            DPRINTF("[naked box] Released read candidate lock\n");
        }
        
    }

    mask = ~mask;
    once = (odds & (~set_2));
    h_pairs = (set_2 & ~set_3);

    struct naked_masks nm = {once,h_pairs,mask};

    if (popcount(h_pairs) < 2){
        nm.h_pair = 0;
    }
    
    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            DPRINTF("[naked box] Acquiring read candidate lock\n");
            rw_monitor_request_read(b->cells[i][j].cs_monitor);
            if ((h_pairs & b->cells[i][j].candidates) == h_pairs)
            {
                h_pairs_count++;
            }
            rw_monitor_release_read(b->cells[i][j].cs_monitor);
            DPRINTF("[naked box] Released read candidate lock\n");
        }
    }

    if ( h_pairs_count < 2)
        nm.h_pair = 0;
    
    for (int i=indexes[0].x;i<indexes[1].x;++i)
    {
        for (int j=indexes[0].y;j<indexes[1].y;++j)
        {
            cell = b->cells[i] + j;
            apply_nh_masks(cell,nm);
        }
    }
    return ;
}

int investigate_row(struct board *b,int r)
{
    struct cell r_cell;
    int mask = 0;
    for (int i=0;i<MAX_NUM;++i)
    {

        rw_monitor_request_read(b->cells[r][i].val_monitor);
        r_cell = b->cells[r][i];
        if (r_cell.has_value)
            mask = set(mask,r_cell.value-1);
        rw_monitor_release_read(b->cells[r][i].val_monitor);

    }

    DPRINTF("Calculated row mask %d\n",mask);
    if ((mask ^ MULTIPOTENT_CANDIDATES) == 0)
        return 0;
    //Invert mask for merge
    mask = ~mask;

    for (int i=0;i<MAX_NUM;++i)
    {
        rw_monitor_request_write(b->cells[r][i].cs_monitor);
        rw_monitor_request_read(b->cells[r][i].val_monitor);
        if (! b->cells[r][i].has_value){
            DPRINTF("Old cell [%d][%d] mask %d\n",r,i,b->cells[r][i].candidates);
            b->cells[r][i].candidates &= mask;
            DPRINTF("New cell [%d][%d] mask %d\n",r,i,b->cells[r][i].candidates);
        }
        rw_monitor_release_write(b->cells[r][i].cs_monitor);
        rw_monitor_release_read(b->cells[r][i].val_monitor);
    }
    return 1;
}


int investigate_col(struct board *b,int c)
{
    struct cell r_cell;
    int mask = 0;
    for (int i=0;i<MAX_NUM;++i)
    {
        rw_monitor_request_read(b->cells[i][c].val_monitor);
        r_cell = b->cells[i][c];
        if (r_cell.has_value)
            mask = set(mask,r_cell.value-1);
        rw_monitor_release_read(b->cells[i][c].val_monitor);
    }

    DPRINTF("Calculated col mask %d\n",mask);
    if ((mask ^ MULTIPOTENT_CANDIDATES) == 0)
        return 0;

    //Invert mask for merge
    mask = ~mask;
    for (int i=0;i<MAX_NUM;++i)
    {
        rw_monitor_request_write(b->cells[i][c].cs_monitor);
        rw_monitor_request_read(b->cells[i][c].val_monitor);
        if (! b->cells[i][c].has_value){
            DPRINTF("Old cell [%d][%d] mask %d\n",i,c,b->cells[i][c].candidates);
            b->cells[i][c].candidates &= mask;
            DPRINTF("New cell [%d][%d] mask %d\n",i,c,b->cells[i][c].candidates);
        }
        rw_monitor_release_write(b->cells[i][c].cs_monitor);
        rw_monitor_release_read(b->cells[i][c].val_monitor);
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
            rw_monitor_request_read(b->cells[i][j].val_monitor);
            DPRINTF("Checking cell [%d][%d]\n",i,j);
            r_cell = b->cells[i][j];
            if (r_cell.has_value)
                mask = set(mask,r_cell.value-1);
            rw_monitor_release_read(b->cells[i][j].val_monitor);
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
            rw_monitor_request_read(b->cells[i][j].val_monitor);
            rw_monitor_request_write(b->cells[i][j].cs_monitor);
            if (! b->cells[i][j].has_value)
                b->cells[i][j].candidates &= mask;
            rw_monitor_release_write(b->cells[i][j].cs_monitor);
            rw_monitor_release_read(b->cells[i][j].val_monitor);
        }
    }

    return 1;
}

int __freeze_cell_state(struct board *b)
{
    struct cell cell;
    int bit_count;
    int has_changed = 0;
    int val;
    for (int i=0;i<MAX_NUM;++i){
        for (int j=0;j<MAX_NUM;++j){
            DPRINTF("Requesting cell [%d][%d] locks\n",i,j);
            rw_monitor_request_read(b->cells[i][j].cs_monitor);
            rw_monitor_request_write(b->cells[i][j].val_monitor);
            DPRINTF("Acquired cell [%d][%d] locks\n",i,j);

            cell = b->cells[i][j];
            bit_count = popcount(cell.candidates);
            DPRINTF("Found cell [%d][%d]  with %d candidates\n",i,j,bit_count);
            if((!cell.has_value) && bit_count == 0){
                DPRINTF("Found cell [%d][%d]  with 0 candidates, can't resolve the sudoku\n",i,j);
                return 0;
            }else if ((!cell.has_value) && bit_count == 1){
                rw_monitor_request_write(b->counter_monitor);
                val = outer_bit_pos(cell.candidates) + 1;
                DPRINTF("Found cell [%d][%d]  with 1 candidates, setting value %d\n",i,j,val);
                b->cells[i][j].value = val;
                b->cells[i][j].has_value = 1;
                has_changed = 1;
                b->unset_cells--;
                rw_monitor_release_write(b->counter_monitor);
            }
            rw_monitor_release_read(b->cells[i][j].cs_monitor);
            rw_monitor_release_write(b->cells[i][j].val_monitor);
        }
    }

    return has_changed;
}

int freeze_cell_state(struct board *b,struct cell *cell)
{
    int bit_count;
    int has_changed = 0;
    int val;
    rw_monitor_request_read(cell->cs_monitor);
    rw_monitor_request_write(cell->val_monitor);
    bit_count = popcount(cell->candidates);
    if((!cell->has_value) && bit_count == 0){
        DPRINTF("Found cell with 0 candidates, can't resolve the sudoku\n");
        rw_monitor_release_read(cell->cs_monitor);
        rw_monitor_release_write(cell->val_monitor);
        return 0;
    }else if ((!cell->has_value) && bit_count == 1){
        val = outer_bit_pos(cell->candidates) + 1;
        DPRINTF("Found cell with 1 candidates, setting value %d\n",val);
        cell->value = val;
        cell->has_value = 1;
        has_changed = 1;
        rw_monitor_request_write(b->counter_monitor);
        b->unset_cells--;
        rw_monitor_release_write(b->counter_monitor);
    }
    rw_monitor_release_read(cell->cs_monitor);
    rw_monitor_release_write(cell->val_monitor);

    return has_changed;

}

void *row_thread_target(void *args)
{
    DPRINTF("Row started\n");
    struct board *b = (struct board *)args;
    int i;
    int has_changed = 0;
    while(b->has_changed){
        for (i=0;i<MAX_NUM;++i){
            DPRINTF("Investigating row [%d]\n",i);
            investigate_row(b,i);
            // Check for naked candidates
        }
        for (i=0;i<MAX_NUM;++i){
            investigate_naked_pair_row(b,i);
            // Check for naked candidates
        }
        //printf("Row investigator is waiting other investigators\n");
        pthread_barrier_wait(&b->investigation_barrier);
        for (i=0;i<3;++i){
            for(int j=0;j<MAX_NUM;++j){
                DPRINTF("Analysing cell [%d][%d] for state consolidation\n",i,j);
                has_changed |= freeze_cell_state(b,&b->cells[i][j]);
            }
        }
        b->has_changed = 0;
        pthread_barrier_wait(&b->freeze_barrier);
        pthread_mutex_lock(&b->change_lock);
        b->has_changed |= has_changed;
        pthread_mutex_unlock(&b->change_lock);
        has_changed = 0;
        DPRINTF("New board state\n");
        DPRINT_BOARD(b);
        pthread_barrier_wait(&b->freeze_barrier);
    }
    DPRINTF("Exiting row investigator thread\n");
    return (void*)b;
}

void *col_thread_target(void *args)
{
    DPRINTF("Column started\n");
    struct board *b = (struct board *)args;
    int has_changed = 0;
    int i;
    while(b->has_changed){
        for (i=0;i<MAX_NUM;++i){
            DPRINTF("Investigating col [%d]\n",i);
            investigate_col(b,i);
        }
        for (i=0;i<MAX_NUM;++i){
            DPRINTF("Investigating col [%d] naked/hidden sets\n",i);
            investigate_naked_pair_col(b,i);
        }
        //printf("Col investigator is waiting other investigators\n");
        pthread_barrier_wait(&b->investigation_barrier);
        for (i=3;i<6;++i){
            for(int j=0;j<MAX_NUM;++j){
                DPRINTF("Analysing cell [%d][%d] for state consolidation\n",i,j);
                has_changed |= freeze_cell_state(b,&b->cells[i][j]);
            }
        }
        pthread_barrier_wait(&b->freeze_barrier);
        pthread_mutex_lock(&b->change_lock);
        b->has_changed |= has_changed;
        pthread_mutex_unlock(&b->change_lock);
        has_changed = 0;
        pthread_barrier_wait(&b->freeze_barrier);
    }
    DPRINTF("Exiting col investigator thread\n");
    return (void*)b;
}

void *box_thread_target(void *args)
{
    DPRINTF("Box started\n");
    struct board *b = (struct board *)args;
    int has_changed = 0;
    int i;
    while(b->has_changed){
        for (i=0;i<MAX_NUM;++i){
            DPRINTF("Investigating box [%d]\n",i);
            investigate_square(b,i);
        }
        for (i=0;i<MAX_NUM;++i){
            DPRINTF("Investigating box [%d] naked/hidden sets\n",i);
            investigate_naked_pair_box(b,i);
        }
        //printf("Box investigator is waiting other investigators\n");
        pthread_barrier_wait(&b->investigation_barrier);
        for (i=6;i<MAX_NUM;++i){
            for(int j=0;j<MAX_NUM;++j){
                DPRINTF("Analysing cell [%d][%d] for state consolidation\n",i,j);
                has_changed |= freeze_cell_state(b,&b->cells[i][j]);
            }
        }
        pthread_barrier_wait(&b->freeze_barrier);
        pthread_mutex_lock(&b->change_lock);
        b->has_changed |= has_changed;
        pthread_mutex_unlock(&b->change_lock);
        has_changed = 0;
        pthread_barrier_wait(&b->freeze_barrier);
    }
    DPRINTF("Exiting box investigator thread\n");
    return (void*)b;
}

/*
 * Solves a board starting with the given cell. Returns 1 if the board could be
 * solved, 0 if not.
 */

struct ThreadGroup *solve_board(struct board *b)
{
    target t_targets[tg_size]; 
    void **args = (void **) malloc(tg_size * sizeof(void *));
    t_targets[2] = &row_thread_target;
    t_targets[1] = &col_thread_target;
    t_targets[0] = &box_thread_target;
    args[0] = b;
    args[1] = b;
    args[2] = b;
    struct ThreadGroup *thread_group = create_thread_group(t_targets, args, tg_size);
    free(args);
    return thread_group;
}

struct ThreadGroup *parallel_sudoku_solver(int **grid, int n){
	struct board *b;
	/* Create an extended grid */
	b = init_board();
	populate_board(b, grid);
	DPRINT_BOARD(b);

    return solve_board(b);
}

int check_sudoku_solved(struct ThreadGroup *workers)
{
    const void *outputs[3];
    struct board *b;
    join_thread_group(workers, outputs);
    b = (struct board*)outputs[0];
    int sudoku_solved = b->unset_cells == 0;
    DPRINT_BOARD(b);
	/* Free everything */
	free_board(b);
    return sudoku_solved;
}

// int sudoku_solver(int **grid, int n)
// {
// 	int is_solved; /* Flag to check if is solved*/
// 	struct board *b;
// 
// 	/* Create an extended grid */
// 	b = init_board();
// 	populate_board(b, grid);
// 	DPRINT_BOARD(b);
// 
// 
//     is_solved = solve_board(b);
// 
//     if (! is_solved){
//         printf("Sudoku not solved\n");
//         DPRINT_BOARD(b);
//     }
// 	/* Convert in the original grid */
// 	board_to_grid(b, grid);
// 
// 	/* Free everything */
// 	free_board(b);
// 
// 	return 0;
// }

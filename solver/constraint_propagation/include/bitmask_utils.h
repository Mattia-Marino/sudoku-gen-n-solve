#ifndef BITMASK_UTILS_H
#define BITMASK_UTILS_H

#include "linked_list.h" 
#include "sudoku_utils.h" 

/**
 * Converts an integer grid (int**) into an extended grid (struct node***).
 * Each cell gets a candidate list:
 *  - If grid[i][j] == 0 → candidates are 1..n
 *  - Otherwise → only the fixed value
 * 
 * @param grid the grid to convert
 * @param n the size of the grid
 * @returns the extended grid
 */
struct node*** extend_grid_with_candidates(int **grid, int n);

/**
 * Converts an extended grid to a bitmask grid.
 * Bit k (0-based) set means candidate (k+1).
 * 
 * @param extended_grid the grid to convert
 * @param n the size of the grid
 * @returns the converted grid
 */
int** convert_extended_to_bitmask_grid(struct node*** extended_grid, int n);

/**
 * Converts a bitmask grid back into an extended linked-list grid.
 * This reconstructs the linked list of candidates from the bitmask.
 * 
 * @param bitmaskgrid the bitmask grid to convert
 * @param n the size of the grid
 * @return the extended grid in the for of a matrix
 * 
 */
struct node*** convert_bitmask_to_extended_grid(int** bitmask_grid, int n);

/**
 * Checks if an extended grid is "solved" in terms of candidates.
 * A cell is considered solved if it has exactly one candidate in its linked list.
 * 
 * @param extended_grid: the grid to check, in the form of a matrix
 * @param n the size of the grid
 * @returns 1 if solved, 0 if not solved
 */
int check_solved_extended(struct node ***extended_grid, int n); 

#endif

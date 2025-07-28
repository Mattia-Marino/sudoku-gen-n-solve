/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../include/debug.h"
#include "../../include/solver.h"
#include "../../include/solver_parallel.h"
#include "../../include/sudoku_utils.h"
#include "../../include/linked_list.h"

/* No longer using work_item_t structure - using clean assignment pattern from new_main.c */

/** 
 * @brief Helper function for master to assign a sudoku task to a slave.
 * Uses clean assignment logic similar to new_main.c
 *
 * @param slave_rank The rank of the slave to assign work to
 * @param collection The sudoku collection
 * @param n Size of sudoku grids
 * @param next_grid_idx_to_start_assigning Pointer to next grid to start assigning
 * @param slaves_assigned_to_grid_count Array tracking slaves per grid
 * @param grid_is_completed Array tracking completed grids
 * @param K_slaves_per_grid Number of slaves needed per grid
 * @returns 1 if a task was assigned, 0 otherwise.
 */
int try_assign_sudoku_task_to_slave(int slave_rank, sudoku_collection_t *collection, int n,
				     int *next_grid_idx_to_start_assigning,
				     int *slaves_assigned_to_grid_count, /* size collection->count */
				     int *grid_is_completed, /* size collection->count */
				     int K_slaves_per_grid)
{
	int i; /* Loop variable */
	int grid_to_send = -1;
	int slave_ordinal_for_grid;
	int component_type; /* 0=rows, 1=columns, 2=boxes */

	/* Priority 1: Find a grid that has been started but needs more slaves (and not completed) */
	for (i = 0; i < collection->count; ++i) {
		if (!grid_is_completed[i] &&
		    slaves_assigned_to_grid_count[i] > 0 &&
		    slaves_assigned_to_grid_count[i] < K_slaves_per_grid) {
			grid_to_send = i;
			break;
		}
	}

	/* Priority 2: If no such grid, find a new grid to start */
	if (grid_to_send == -1 && *next_grid_idx_to_start_assigning < collection->count) {
		if (!grid_is_completed[*next_grid_idx_to_start_assigning] &&
		    slaves_assigned_to_grid_count[*next_grid_idx_to_start_assigning] == 0) {
			grid_to_send = *next_grid_idx_to_start_assigning;
		}
	}

	if (grid_to_send != -1) {
		/* Determine component type based on current slave count for this grid */
		slave_ordinal_for_grid = slaves_assigned_to_grid_count[grid_to_send] + 1;
		component_type = slaves_assigned_to_grid_count[grid_to_send] % K_slaves_per_grid; /* 0, 1, 2 for rows, cols, boxes */
		
		/* Send grid index to slave */
		MPI_Send(&grid_to_send, 1, MPI_INT, slave_rank, 0, MPI_COMM_WORLD);
		
		/* Send component type and slave ordinal */
		MPI_Send(&component_type, 1, MPI_INT, slave_rank, 1, MPI_COMM_WORLD);
		MPI_Send(&slave_ordinal_for_grid, 1, MPI_INT, slave_rank, 2, MPI_COMM_WORLD);
		
		/* Send the actual grid */
		int **grid = get_grid_from_collection(collection, grid_to_send);
		if (grid != NULL) {
			for (i = 0; i < n; i++) {
				MPI_Send(grid[i], n, MPI_INT, slave_rank, 3 + i, MPI_COMM_WORLD);
			}
		}
		
		/* Update count AFTER calculating component type */
		slaves_assigned_to_grid_count[grid_to_send]++;
		printf("Master: Assigned grid %d (component type %d) to slave %d (this is slave %d/%d for this grid).\n",
		       grid_to_send, component_type, slave_rank,
		       slaves_assigned_to_grid_count[grid_to_send], K_slaves_per_grid);

		/* If this assignment makes the current 'next_grid_idx_to_start_assigning' fully assigned, advance the pointer. */
		if (slaves_assigned_to_grid_count[grid_to_send] == K_slaves_per_grid &&
		    grid_to_send == *next_grid_idx_to_start_assigning) {
			(*next_grid_idx_to_start_assigning)++;
		}
		return 1;
	}
	return 0; /* No task assigned */
}

int main(int argc, char **argv)
{
	char *filename;
	int i, j; /* Loop variables */
	int n = 0;    /* Matrix size */
	int sqrt_n;
	int tot_solved = 0;
	int **grid;
	sudoku_collection_t *collection = NULL;
	clock_t start_time;
	clock_t end_time;
	double computation_time;

	/* Master variables */
	int num_slaves;
	int s_rank;
	int terminate_signal = -1;
	int comp; /* Component type for slaves */
	int total_changes_made = 0; /* Track if any slave made changes */
	MPI_Status status;

	/* Slaves variables */
	int current_grid_idx, current_component_type, current_slave_ordinal;
	int **slave_grid = NULL;
	int changes_made;
	int depth_idx;
	int depth;

	int rank, size;
	const int K_SLAVES_PER_GRID = 3; /* rows, columns, boxes */

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* Broadcast the problem size to all processes */
	if (rank == 0) {
		if (argc != 3) {
			fprintf(stderr, "Usage: %s <size> <filename>\n", argv[0]);
			MPI_Finalize();
			return 1;
		}
		n = atoi(argv[1]);
	}
	MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

	/* Master code */
	if(rank == 0){
		/* Read the size of the file from command line */
		/* n already set above from argv[1] */

		/* Checks on puzzle size */
		if (n < 1) {
			printf("Master: n is 0 or negative, no work to do.\n");
			num_slaves = size - 1;
			if (num_slaves > 0)
				for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
					terminate_signal = -1;
					MPI_Send(&terminate_signal, 1, MPI_INT,
						 s_rank, 0, MPI_COMM_WORLD);
				}
		}

		sqrt_n = (int)sqrt(n);
		if (sqrt_n * sqrt_n != n) {
			fprintf(stderr, "Error: Size %d must be a perfect square (4, 9, 16, ...)\n", n);
			MPI_Finalize();
			return 1;
		}

		/* Open and read the file */
		filename = argv[2];
		collection = read_all_sudokus_from_file(filename, n);
		if(collection == NULL) {
			fprintf(stderr, "Error: Failed to read Sudoku grids from file %s\n", filename);
			MPI_Finalize();
			return 1;
		}
		
		start_time = clock();
		
		/* Calculate number of available slaves */
		num_slaves = size - 1;
		
		if (num_slaves == 0) {
			printf("No slaves available. Master will solve all grids sequentially.\n");
			for (i = 0; i < collection->count; i++) {
				grid = get_grid_from_collection(collection, i);
				DPRINTF("Solving grid %d...\n", i);
				DPRINT_SUDOKU(grid, n);
				sudoku_solver(grid, n);
				if (check_solved(grid, n)) {
					tot_solved++;
				}
			}
		}else if (num_slaves > 0 && num_slaves < K_SLAVES_PER_GRID) {
			fprintf(stderr,
					"Master: Error - Not enough slaves (%d) to assign %d per grid. Need at least %d slaves.\n",
					num_slaves, K_SLAVES_PER_GRID,
					K_SLAVES_PER_GRID);
				for (s_rank = 1; s_rank <= num_slaves;
				     ++s_rank) {
					MPI_Send(&terminate_signal, 1, MPI_INT,
						 s_rank, 0, MPI_COMM_WORLD);
		}
		} else {
			/* Master-slave logic using clean assignment pattern */
			int *slaves_assigned_to_grid_count = (int *)calloc(collection->count, sizeof(int));
			int *grid_is_completed = (int *)calloc(collection->count, sizeof(int));
			int **grid_components_completed = (int **)malloc(collection->count * sizeof(int *)); /* Track which components completed per grid */
			int next_grid_idx_to_start_assigning = 0;
			int **temp_received_grid = NULL;

			if (slaves_assigned_to_grid_count == NULL || grid_is_completed == NULL || grid_components_completed == NULL) {
				fprintf(stderr, "Master: Failed to allocate tracking arrays.\n");
				if (slaves_assigned_to_grid_count) free(slaves_assigned_to_grid_count);
				if (grid_is_completed) free(grid_is_completed);
				if (grid_components_completed) free(grid_components_completed);
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
			}

			/* Initialize component tracking arrays */
			for (i = 0; i < collection->count; i++) {
				grid_components_completed[i] = (int *)calloc(K_SLAVES_PER_GRID, sizeof(int)); /* 0=rows, 1=cols, 2=boxes */
				if (grid_components_completed[i] == NULL) {
					fprintf(stderr, "Master: Failed to allocate component tracking array for grid %d.\n", i);
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
					return EXIT_FAILURE;
				}
			}

			/* Allocate temp grid for receiving results */
			temp_received_grid = create_grid(n);
			if (temp_received_grid == NULL) {
				fprintf(stderr, "Master: Failed to allocate temp_received_grid.\n");
				free(slaves_assigned_to_grid_count);
				free(grid_is_completed);
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
			}

			printf("Master: Starting parallel processing with %d slaves\n", num_slaves);
			
			/* Phase 1: Initial dispatch to slaves */
			int slaves_given_work = 0;
			for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
				int assigned = try_assign_sudoku_task_to_slave(
					s_rank, collection, n,
					&next_grid_idx_to_start_assigning,
					slaves_assigned_to_grid_count,
					grid_is_completed, K_SLAVES_PER_GRID);
				if (assigned) {
					slaves_given_work++;
				} else {
					/* This slave couldn't be assigned a task initially */
					MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
				}
			}
			printf("Master: Initially assigned work to %d slaves\n", slaves_given_work);

			/* Phase 2: Iterative rounds with slave communication */
			int round = 0;
			int max_rounds = 5; /* Maximum iterative rounds */
			int global_changes_this_round = 0;
			
			do {
				round++;
				global_changes_this_round = 0;
				printf("Master: Starting iterative round %d\n", round);
				
				/* Reset component completion tracking for this round */
				for (i = 0; i < collection->count; i++) {
					for (comp = 0; comp < K_SLAVES_PER_GRID; comp++) {
						grid_components_completed[i][comp] = 0;
					}
				}
				
				/* Collect results from all active slaves */
				int slaves_completed_this_round = 0;
				while (slaves_completed_this_round < slaves_given_work) {
					/* Receive completed work from a slave */
					int received_grid_idx, received_component_type, received_slave_ordinal;
					int changes_flag;

					MPI_Recv(&received_grid_idx, 1, MPI_INT, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &status);
					MPI_Recv(&received_component_type, 1, MPI_INT, status.MPI_SOURCE, 11, MPI_COMM_WORLD, &status);
					MPI_Recv(&received_slave_ordinal, 1, MPI_INT, status.MPI_SOURCE, 12, MPI_COMM_WORLD, &status);
					MPI_Recv(&changes_flag, 1, MPI_INT, status.MPI_SOURCE, 13, MPI_COMM_WORLD, &status);

					/* Receive the processed grid back */
					for (i = 0; i < n; i++) {
						MPI_Recv(temp_received_grid[i], n, MPI_INT, status.MPI_SOURCE, 14 + i, MPI_COMM_WORLD, &status);
					}

					int reporting_slave_rank = status.MPI_SOURCE;

					/* Update the grid in collection with received results */
					grid = get_grid_from_collection(collection, received_grid_idx);
					for (i = 0; i < n; i++) {
						for (j = 0; j < n; j++) {
							grid[i][j] = temp_received_grid[i][j];
						}
					}

					if (changes_flag) {
						total_changes_made++;
						global_changes_this_round++;
						printf("Master: Round %d - Slave %d made changes to grid %d (component type %d)\n",
						       round, reporting_slave_rank, received_grid_idx, received_component_type);
					} else {
						printf("Master: Round %d - Slave %d made no changes to grid %d (component type %d)\n",
						       round, reporting_slave_rank, received_grid_idx, received_component_type);
					}

					/* Mark this component as completed for this round */
					grid_components_completed[received_grid_idx][received_component_type] = 1;
					slaves_completed_this_round++;
				}
				
				printf("Master: Round %d completed with %d total changes\n", round, global_changes_this_round);
				
				/* Check if we should continue iterating */
				if (global_changes_this_round > 0 && round < max_rounds) {
					printf("Master: Changes detected, starting next round...\n");
					
					/* Send updated grids back to slaves for next round */
					for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
						/* Find which grid this slave was working on */
						int slave_grid_idx = -1;
						int slave_component_type = -1;
						
						/* Simple assignment: slave 1->grid 0 rows, slave 2->grid 0 cols, slave 3->grid 0 boxes, then cycle */
						slave_grid_idx = ((s_rank - 1) / K_SLAVES_PER_GRID) % collection->count;
						slave_component_type = (s_rank - 1) % K_SLAVES_PER_GRID;
						
						if (slave_grid_idx < collection->count && !grid_is_completed[slave_grid_idx]) {
							/* Send grid index */
							MPI_Send(&slave_grid_idx, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
							
							/* Send component type and slave ordinal */
							MPI_Send(&slave_component_type, 1, MPI_INT, s_rank, 1, MPI_COMM_WORLD);
							int slave_ordinal = ((s_rank - 1) % K_SLAVES_PER_GRID) + 1;
							MPI_Send(&slave_ordinal, 1, MPI_INT, s_rank, 2, MPI_COMM_WORLD);
							
							/* Send the updated grid */
							grid = get_grid_from_collection(collection, slave_grid_idx);
							if (grid != NULL) {
								for (i = 0; i < n; i++) {
									MPI_Send(grid[i], n, MPI_INT, s_rank, 3 + i, MPI_COMM_WORLD);
								}
							}
						} else {
							/* No more work for this slave */
							MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
							slaves_given_work--; /* Reduce count for next round */
						}
					}
				} else {
					/* No more changes or max rounds reached - terminate all slaves */
					printf("Master: No more changes or max rounds reached, terminating slaves...\n");
					for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
						MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
					}
					break;
				}
				
			} while (global_changes_this_round > 0 && round < max_rounds);

			/* Clean up */
			free(slaves_assigned_to_grid_count);
			free(grid_is_completed);
			for (i = 0; i < collection->count; i++) {
				free(grid_components_completed[i]);
			}
			free(grid_components_completed);
			free_grid(temp_received_grid, n);
		}
			/* Count solved grids */
			printf("Master: Checking final status of all grids...\n");
			for (i = 0; i < collection->count; i++) {
				grid = get_grid_from_collection(collection, i);
				if (check_solved(grid, n)) {
					tot_solved++;
					printf("Master: Grid %d - SOLVED\n", i);
				} else {
					printf("Master: Grid %d - NOT SOLVED\n", i);
				}
				DPRINT_SUDOKU(grid, n);
			}
			printf("Master: Total grids solved: %d out of %d\n", tot_solved, collection->count);
			printf("Master: Total constraint propagation changes made: %d\n", total_changes_made);
		
		printf("\nMaster: Work distribution summary:\n");
		printf("- Total grids: %d\n", collection->count);
		printf("- Total slaves: %d\n", num_slaves);
		printf("- Total constraint propagation changes made: %d\n", total_changes_made);
		
		end_time = clock();
		computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

		printf("\nTotal computation completed in %.6f seconds.\n",
		computation_time);

		printf("Sudokus completely solved: %d\n\n", tot_solved);
		
		/* Clean up */
		free_sudoku_collection(collection);
	} else { /* Slave processes (rank > 0) */
		
		/* Wait for first work assignment or termination signal */
		MPI_Recv(&current_grid_idx, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
		
		if (current_grid_idx != terminate_signal && n > 0) {
			/* Allocate memory for the grid */
			slave_grid = create_grid(n);
			if (slave_grid == NULL) {
				fprintf(stderr, "Slave %d: Failed to allocate grid memory\n", rank);
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
			}
		}
		
		while (current_grid_idx != terminate_signal) {
			if (n == 0) { /* Should have been caught by terminate_signal, but as a safeguard */
				break;
			}

			/* Receive component type and slave ordinal */
			MPI_Recv(&current_component_type, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
			MPI_Recv(&current_slave_ordinal, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, &status);
			
			/* Receive the grid */
			for (i = 0; i < n; i++) {
				MPI_Recv(slave_grid[i], n, MPI_INT, 0, 3 + i, MPI_COMM_WORLD, &status);
			}
			
			printf("Slave %d: Processing grid %d, component type %d (slave ordinal %d)\n",
			       rank, current_grid_idx, current_component_type, current_slave_ordinal);
			
			/* Create extended grid and process using existing parallel solver functions */
			struct node ***extended_grid = extend_grid(slave_grid, n);
			if (extended_grid == NULL) {
				fprintf(stderr, "Slave %d: Failed to create extended grid\n", rank);
				changes_made = 0;
			} else {
				int sqrt_n = (int)sqrt(n);
				int max_depth = (int)floor((double)n / 2);
				int ***already_propagated = (int ***)malloc(max_depth * sizeof(int **));
				
				for (depth_idx = 0; depth_idx < max_depth; depth_idx++) {
					already_propagated[depth_idx] = (int **)malloc(n * sizeof(int *));
					for (j = 0; j < n; j++) {
						already_propagated[depth_idx][j] = (int *)malloc(n * sizeof(int));
					}
					initialize_propagation_matrix(already_propagated[depth_idx], n);
				}
				
				/* Perform constraint propagation using existing parallel solver functions */
				printf("Slave %d: Starting constraint propagation for grid %d, component type %d\n", 
				       rank, current_grid_idx, current_component_type);
				
				changes_made = 0;
				
				/* Use iterative approach with slave communication */
				int iteration = 0;
				int max_iterations = 10; /* Limit iterations to prevent infinite loops */
				
				do {
					int local_changes = 0;
					iteration++;
					
					printf("Slave %d: Starting iteration %d for grid %d\n", rank, iteration, current_grid_idx);
					
					/* Process based on component type using existing parallel solver functions */
					switch (current_component_type) {
						case 0: /* Row constraint propagation */
							printf("Slave %d: Performing row constraint propagation (iteration %d)\n", rank, iteration);
							for (depth = 1; depth <= max_depth; depth++) {
								local_changes += parallel_naked_candidates_rows(extended_grid, n, 
									already_propagated[depth-1], depth, 0, sqrt_n);
							}
							break;
							
						case 1: /* Column constraint propagation */
							printf("Slave %d: Performing column constraint propagation (iteration %d)\n", rank, iteration);
							for (depth = 1; depth <= max_depth; depth++) {
								local_changes += parallel_naked_candidates_cols(extended_grid, n, 
									already_propagated[depth-1], depth, 0, sqrt_n);
							}
							break;
							
						case 2: /* Box constraint propagation */
							printf("Slave %d: Performing box constraint propagation (iteration %d)\n", rank, iteration);
							for (depth = 1; depth <= max_depth; depth++) {
								local_changes += parallel_naked_candidates_boxes(extended_grid, n, 
									already_propagated[depth-1], depth, 0, sqrt_n);
							}
							break;
							
						default:
							printf("Slave %d: Unknown component type %d\n", rank, current_component_type);
							break;
					}
					
					printf("Slave %d: Made %d local changes in iteration %d\n", rank, local_changes, iteration);
					changes_made += local_changes;
					
					/* Simplified slave communication: just use barriers for synchronization */
					/* This avoids deadlocks while still allowing some coordination */
					if (local_changes > 0) {
						printf("Slave %d: Changes made, continuing to next iteration\n", rank);
					}
					
					/* Stop if no changes or max iterations reached */
					if (local_changes == 0 || iteration >= max_iterations) {
						printf("Slave %d: Stopping iterations - local_changes: %d, iteration: %d\n", 
						       rank, local_changes, iteration);
						break;
					}
					
				} while (iteration < max_iterations);
				
				printf("Slave %d: Constraint propagation completed after %d iterations, total changes: %d\n", 
				       rank, iteration, changes_made);
				
				/* Convert extended grid back to regular grid */
				for (i = 0; i < n; i++) {
					for (j = 0; j < n; j++) {
						struct node *temp = extended_grid[i][j];
						if (temp != NULL && temp->next == NULL) {
							slave_grid[i][j] = temp->data; /* Single candidate = solved */
						}
					}
				}
				
				/* Clean up */
				free_extended_grid(extended_grid, n);
				for (depth_idx = 0; depth_idx < max_depth; depth_idx++) {
					for (j = 0; j < n; j++) {
						free(already_propagated[depth_idx][j]);
					}
					free(already_propagated[depth_idx]);
				}
				free(already_propagated);
			}
			
			if (changes_made > 0) {
				printf("Slave %d: Made %d changes to grid %d (component type %d)\n", 
				       rank, changes_made, current_grid_idx, current_component_type);
			} else {
				printf("Slave %d: No changes made to grid %d (component type %d)\n", 
				       rank, current_grid_idx, current_component_type);
			}
			
			/* Send results back to master */
			MPI_Send(&current_grid_idx, 1, MPI_INT, 0, 10, MPI_COMM_WORLD);
			MPI_Send(&current_component_type, 1, MPI_INT, 0, 11, MPI_COMM_WORLD);
			MPI_Send(&current_slave_ordinal, 1, MPI_INT, 0, 12, MPI_COMM_WORLD);
			
			/* Send whether changes were made */
			int changes_flag = changes_made ? 1 : 0;
			MPI_Send(&changes_flag, 1, MPI_INT, 0, 13, MPI_COMM_WORLD);
			
			/* Send the processed grid back */
			for (i = 0; i < n; i++) {
				MPI_Send(slave_grid[i], n, MPI_INT, 0, 14 + i, MPI_COMM_WORLD);
			}
			
			/* Wait for next assignment or termination */
			MPI_Recv(&current_grid_idx, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
		}
		
		/* Clean up */
		if (slave_grid != NULL) {
			free_grid(slave_grid, n);
		}
		
		printf("Slave %d: Terminating\n", rank);
	}

	MPI_Finalize();
	return 0;
}

	


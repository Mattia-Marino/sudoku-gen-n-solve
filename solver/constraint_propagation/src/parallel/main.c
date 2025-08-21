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
#include "../../include/bitmask_utils.h"    


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
	int i, j; /* Loop variable */
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
		
		/* Send the actual grid to slave — as BITMASK grid */
		struct node ***ext = get_extended_grid_from_collection(collection, grid_to_send);
		if (!ext) {
			int **int_grid = get_grid_from_collection(collection, grid_to_send);
			ext = extend_grid_with_candidates(int_grid, n);
			set_extended_grid_in_collection(collection, grid_to_send, ext);
		}
		int **bitmask_to_send = convert_extended_to_bitmask_grid(ext, n);
		if (bitmask_to_send != NULL) {
			for (i = 0; i < n; i++) {
				MPI_Send(bitmask_to_send[i], n, MPI_INT, slave_rank, 3 + i, MPI_COMM_WORLD);
			}
			free_grid(bitmask_to_send, n);
		} else {
			/* Fallback: send an all-candidates grid if conversion failed */
			int **fallback = create_grid(n);
			for (i = 0; i < n; ++i) {
				for (j = 0; j < n; ++j) fallback[i][j] = (1 << n) - 1;
					MPI_Send(fallback[i], n, MPI_INT, slave_rank, 3 + i, MPI_COMM_WORLD);
			}
			free_grid(fallback, n);
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

int int_log2(int x) {
	int result = 0;
	while (x > 1) {
		x >>= 1;
		result++;
	}
	return result;
}

int main(int argc, char **argv)
{
	char *filename;
	int i, j, k, l, r, c, rr; /* Loop variables */
	int n = 0;
	int sqrt_n;
	int tot_solved = 0;
	sudoku_collection_t *collection = NULL;
	clock_t start_time;
	clock_t end_time;
	double computation_time;

	/* Master variables */
	int num_slaves;
	int s_rank;
	int terminate_signal = -1;
	int comp;
	int bit;
	int total_changes_made = 0; 
	MPI_Status status;

	/* Slave variables */
	int current_grid_idx, current_component_type, current_slave_ordinal;
	struct node ***slave_extended_grid = NULL; /* Slave works on extended grid */
	int changes_made; /* Total changes made by this slave in current task */
	int depth_idx;
	int rank, size;
	const int K_SLAVES_PER_GRID = 3; /* One slave for rows, one for columns, one for boxes */

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (rank == 0) {
		if (argc != 3) {
			fprintf(stderr, "Usage: %s <size> <filename>\n", argv[0]);
			MPI_Finalize();
			return 1;
		}
		n = atoi(argv[1]);
	}
	MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

	if (rank == 0) {
		if (n < 1) {
			printf("Master: n is 0 or negative, no work to do.\n");
			num_slaves = size - 1;
			if (num_slaves > 0) {
				for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
				MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
				}
			}
			MPI_Finalize();
			return 0;
		}

		sqrt_n = (int)sqrt(n);
		if (sqrt_n * sqrt_n != n) {
			fprintf(stderr, "Error: Size %d must be a perfect square (4, 9, 16, ...)\n", n);
			MPI_Finalize();
			return 1;
		}

		filename = argv[2];
		collection = read_all_sudokus_from_file(filename, n);
		if (collection == NULL) {
			fprintf(stderr, "Error: Failed to read Sudoku grids from file %s\n", filename);
			MPI_Finalize();
			return 1;
		}

		start_time = clock();
		num_slaves = size - 1;

		if (num_slaves == 0) {
			printf("No slaves available. Master will solve all grids sequentially.\n");
			for (i = 0; i < collection->count; i++) {
					int **original_int_grid = collection->grids[i]; 
					DPRINTF("Solving grid %d...\n", i);
					DPRINT_SUDOKU(original_int_grid, n);
					sudoku_solver(original_int_grid, n); 
					if (check_solved(original_int_grid, n)) { 
						tot_solved++;
				}
			}
		} else if (num_slaves < K_SLAVES_PER_GRID) {
			fprintf(stderr,
				"Master: Error - Not enough slaves (%d) to assign %d per grid. Need at least %d slaves.\n",
				num_slaves, K_SLAVES_PER_GRID, K_SLAVES_PER_GRID);
			for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
				MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
			}
			MPI_Finalize();
			return 1;
		} else {
			int *slaves_assigned_to_grid_count = (int *)calloc(collection->count, sizeof(int));
			int *grid_is_completed = (int *)calloc(collection->count, sizeof(int));
			int **grid_components_completed = (int **)malloc(collection->count * sizeof(int *));
			int next_grid_idx_to_start_assigning = 0;
			int **temp_received_bitmask_grid = NULL;


			/* Check if allocation was successful */
			if (slaves_assigned_to_grid_count == NULL || grid_is_completed == NULL || grid_components_completed == NULL) {
				fprintf(stderr, "Master: Failed to allocate tracking arrays.\n");
				if (slaves_assigned_to_grid_count) free(slaves_assigned_to_grid_count);
				if (grid_is_completed) free(grid_is_completed);
				if (grid_components_completed) {
					for(k=0; k<collection->count; ++k) free(grid_components_completed[k]);
					free(grid_components_completed);
				}
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
			}

			/* Initialize component tracking arrays */
			for (i = 0; i < collection->count; i++) {
				grid_components_completed[i] = (int *)calloc(K_SLAVES_PER_GRID, sizeof(int));
				if (grid_components_completed[i] == NULL) {
					fprintf(stderr, "Master: Failed to allocate component tracking array for grid %d.\n", i);
					for(k=0; k<i; ++k) free(grid_components_completed[k]);
						free(slaves_assigned_to_grid_count);
						free(grid_is_completed);
						free(grid_components_completed);
						MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
						return EXIT_FAILURE;
				}
			}

			/* Initiliaze bitmask grid */
			temp_received_bitmask_grid = create_grid(n);
			if (temp_received_bitmask_grid == NULL) {
				fprintf(stderr, "Master: Failed to allocate temp_received_bitmask_grid.\n");
				free(slaves_assigned_to_grid_count);
				free(grid_is_completed);
				for (i = 0; i < collection->count; i++) free(grid_components_completed[i]);
					free(grid_components_completed);
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
					return EXIT_FAILURE;
			}

			printf("Master: Starting parallel processing with %d slaves\n", num_slaves);

			/* Assign work to slaves */
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
						MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
				}
			}
			printf("Master: Initially assigned work to %d slaves\n", slaves_given_work);

			int round = 0;
			int max_rounds = 50;
			int global_changes_this_round = 0;

			if (slaves_given_work > 0 && collection->count > 0) {
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

					int slaves_completed_this_round = 0;
					while (slaves_completed_this_round < slaves_given_work) {
						int received_grid_idx, received_component_type, received_slave_ordinal;
						int changes_flag_from_slave; 

						/* Receive data from slave */
						MPI_Recv(&received_grid_idx, 1, MPI_INT, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &status);
						MPI_Recv(&received_component_type, 1, MPI_INT, status.MPI_SOURCE, 11, MPI_COMM_WORLD, &status);
						MPI_Recv(&received_slave_ordinal, 1, MPI_INT, status.MPI_SOURCE, 12, MPI_COMM_WORLD, &status);
						MPI_Recv(&changes_flag_from_slave, 1, MPI_INT, status.MPI_SOURCE, 13, MPI_COMM_WORLD, &status); /* Now receives total changes count */

						for (i = 0; i < n; i++) {
							MPI_Recv(temp_received_bitmask_grid[i], n, MPI_INT, status.MPI_SOURCE, 14 + i, MPI_COMM_WORLD, &status);
						}

						int reporting_slave_rank = status.MPI_SOURCE;

						struct node ***master_current_ext_grid = get_extended_grid_from_collection(collection, received_grid_idx);
						int candidates_changed_by_merge = 0;

						/* Master: MERGE received bitmasks with its current global extended grid */
						for (i = 0; i < n; i++) {
							for (j = 0; j < n; j++) {
								int master_current_mask = 0;
								struct node *node_ptr = master_current_ext_grid[i][j];
								while (node_ptr != NULL) {
									if (node_ptr->data >= 1 && node_ptr->data <= n) {
										master_current_mask |= (1 << (node_ptr->data - 1));
									}
									node_ptr = node_ptr->next;
								}

								/* Bitwise AND*/
								int new_merged_mask = master_current_mask & temp_received_bitmask_grid[i][j];

								if (new_merged_mask != master_current_mask) {
									candidates_changed_by_merge += __builtin_popcount(master_current_mask) - __builtin_popcount(new_merged_mask); /* Count actual candidate eliminations */
									free_list(master_current_ext_grid[i][j]);
									master_current_ext_grid[i][j] = NULL;

									for (bit = 0; bit < n; bit++) {
										if ((new_merged_mask >> bit) & 1) {
											master_current_ext_grid[i][j] = append(master_current_ext_grid[i][j], bit + 1);
										}
									}
								}
							}
						}

						if (changes_flag_from_slave > 0 || candidates_changed_by_merge > 0) {
							/* Add slave's changes and master's merge changes */
							total_changes_made += changes_flag_from_slave + candidates_changed_by_merge; 
							global_changes_this_round += changes_flag_from_slave + candidates_changed_by_merge;
							printf("Master: Round %d - Slave %d reported %d changes to grid %d (type %d), master merged %d more changes. Total global this round: %d\n",
								round, reporting_slave_rank, changes_flag_from_slave, received_grid_idx, received_component_type, candidates_changed_by_merge, global_changes_this_round);
						} else {
							printf("Master: Round %d - Slave %d made no changes to grid %d (type %d), no master merge changes. Total global this round: %d\n",
								round, reporting_slave_rank, received_grid_idx, received_component_type, global_changes_this_round);
						}

						grid_components_completed[received_grid_idx][received_component_type] = 1;
						slaves_completed_this_round++;

						int assigned_new_work = try_assign_sudoku_task_to_slave(
						reporting_slave_rank, collection, n,
						&next_grid_idx_to_start_assigning,
						slaves_assigned_to_grid_count,
						grid_is_completed, K_SLAVES_PER_GRID);

						if (!assigned_new_work) {
							MPI_Send(&terminate_signal, 1, MPI_INT, reporting_slave_rank, 0, MPI_COMM_WORLD);
							slaves_given_work--;
							printf("Master: Terminating slave %d due to no more work available.\n", reporting_slave_rank);
						}
					}

					printf("Master: Round %d completed with %d global changes in candidate counts.\n", round, global_changes_this_round);

					int any_grid_unsolved = 0;
					for(i=0; i<collection->count; ++i) {
						if(!check_solved_extended(get_extended_grid_from_collection(collection, i), n)) {
							any_grid_unsolved = 1;
							break;
						}
					}

					if (global_changes_this_round > 0 && round < max_rounds && any_grid_unsolved) {
						printf("Master: Changes detected and unsolved grids remain, starting next round...\n");
					} else {
						printf("Master: No more global changes, max rounds reached, or all grids solved. Terminating remaining slaves...\n");
						for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
							/* Ensure all currently active slaves get termination signal */
							MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
						}
						break;
					}
				} while (slaves_given_work > 0);
			} else {
				printf("Master: No initial work was assigned to any slave or no grids to solve.\n");
				for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
					MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
				}
			}

			free(slaves_assigned_to_grid_count);
			free(grid_is_completed);
			for (i = 0; i < collection->count; i++) {
				if(grid_components_completed[i]) free(grid_components_completed[i]);
			}
			free(grid_components_completed);
			free_grid(temp_received_bitmask_grid, n);
		}

		printf("Master: Checking final status of all grids...\n");
		for (i = 0; i < collection->count; i++) {
			struct node*** final_ext_grid = get_extended_grid_from_collection(collection, i);
			if (check_solved_extended(final_ext_grid, n)) {
				tot_solved++;
				printf("Master: Grid %d - SOLVED\n", i);
				int** final_int_grid_for_print = convert_extended_to_bitmask_grid(final_ext_grid, n);
				for(r=0; r<n; ++r) {
				for(c=0; c<n; ++c) {
					int mask = final_int_grid_for_print[r][c];
					/* Check if only one bit is set (power of 2) */
					if (mask && (mask & (mask - 1)) == 0) { 
						/* Convert bit to value */
						final_int_grid_for_print[r][c] = int_log2(mask) + 1; 
					} else {
						/* Display as 0 if not uniquely solved */
						final_int_grid_for_print[r][c] = 0; 
					}
				}
				}
				DPRINT_SUDOKU(final_int_grid_for_print, n);
				printf("Here the final grid:\n");
				display_sudoku(final_int_grid_for_print, n);
				free_grid(final_int_grid_for_print, n);
			} else {
				printf("Master: Grid %d - NOT SOLVED\n", i);
				int** partial_int_grid_for_print = convert_extended_to_bitmask_grid(final_ext_grid, n);
				for(r=0; r<n; ++r) {
					for(c=0; c<n; ++c) {
						int mask = partial_int_grid_for_print[r][c];
						if (mask && (mask & (mask - 1)) == 0) {
							partial_int_grid_for_print[r][c] = int_log2(mask) + 1;
						} else {
							partial_int_grid_for_print[r][c] = 0;
						}
					}
				}
				DPRINT_SUDOKU(partial_int_grid_for_print, n);
				printf("Here's the partial result:\n");
				display_sudoku(partial_int_grid_for_print, n);
				free_grid(partial_int_grid_for_print, n);
			}
		}
		printf("Master: Total grids solved: %d out of %d\n", tot_solved, collection->count);
		printf("Master: Total constraint propagation changes made: %d\n", total_changes_made);

		printf("\nMaster: Work distribution summary:\n");
		printf("- Total grids: %d\n", collection->count);
		printf("- Total slaves: %d\n", num_slaves);
		printf("- Total constraint propagation changes made: %d\n", total_changes_made);

		end_time = clock();
		computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

		printf("\nTotal computation completed in %.6f seconds.\n", computation_time);
		printf("Sudokus completely solved: %d\n\n", tot_solved);

		free_sudoku_collection(collection);
	} else { /* Slave processes (rank > 0) */
		int **temp_received_bitmask_grid = NULL;
		slave_extended_grid = NULL;

		MPI_Recv(&current_grid_idx, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

		if (current_grid_idx != terminate_signal && n > 0 && current_grid_idx != -2) {
			temp_received_bitmask_grid = create_grid(n);
			if (temp_received_bitmask_grid == NULL) {
				fprintf(stderr, "Slave %d: Failed to allocate temp_received_bitmask_grid\n", rank);
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			}
		}

		while (current_grid_idx != terminate_signal) {
			if (n == 0 || current_grid_idx == -2) {
				fprintf(stderr, "Slave %d: Received special signal (n=0 or error), terminating.\n", rank);
				break;
			}

			MPI_Recv(&current_component_type, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
			MPI_Recv(&current_slave_ordinal, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, &status);

			for (i = 0; i < n; i++) {
				MPI_Recv(temp_received_bitmask_grid[i], n, MPI_INT, 0, 3 + i, MPI_COMM_WORLD, &status);
			}

			printf("Slave %d: Processing grid %d, component type %d (slave ordinal %d)\n",
				rank, current_grid_idx, current_component_type, current_slave_ordinal);

			if (slave_extended_grid) {
				free_extended_grid(slave_extended_grid, n);
			}
			slave_extended_grid = convert_bitmask_to_extended_grid(temp_received_bitmask_grid, n);
			
			if (slave_extended_grid == NULL) {
				fprintf(stderr, "Slave %d: Failed to convert bitmask grid to extended grid\n", rank);
				changes_made = 0;
			} else {
				int max_depth_for_prop = (int)floor((double)n / 2.0); /* Use 2.0 for float division */
				int ***already_propagated = NULL;

				if (max_depth_for_prop > 0) {
					already_propagated = (int ***)malloc(max_depth_for_prop * sizeof(int **));
					if (already_propagated == NULL) {
						fprintf(stderr, "Slave %d: Failed to allocate already_propagated array.\n", rank);
						free_extended_grid(slave_extended_grid, n);
						slave_extended_grid = NULL;
					} else {
						for (depth_idx = 0; depth_idx < max_depth_for_prop; depth_idx++) {
							already_propagated[depth_idx] = (int **)malloc(n * sizeof(int *));
							if (already_propagated[depth_idx] == NULL) {
								fprintf(stderr, "Slave %d: Failed to allocate already_propagated[%d] row pointers.\n", rank, depth_idx);
								for(k=0; k<depth_idx; ++k) {
									for(l=0; l<n; ++l) free(already_propagated[k][l]);
										free(already_propagated[k]);
								}
								free(already_propagated);
								already_propagated = NULL;
								break;
							}
							for (j = 0; j < n; j++) {
								already_propagated[depth_idx][j] = (int *)calloc(n, sizeof(int));
								if (already_propagated[depth_idx][j] == NULL) {
									fprintf(stderr, "Slave %d: Failed to allocate already_propagated[%d][%d] column.\n", rank, depth_idx, j);
									for(l=0; l<j; ++l) free(already_propagated[depth_idx][l]);
										free(already_propagated[depth_idx]);
									for(k=0; k<depth_idx; ++k) {
										for(l=0; l<n; ++l) free(already_propagated[k][l]);
										free(already_propagated[k]);
									}
									free(already_propagated);
									already_propagated = NULL;
									break;
								}
							}
						}
					}
				}
				
				changes_made = 0;
				if (slave_extended_grid != NULL && (max_depth_for_prop == 0 || already_propagated != NULL)) {
					printf("Slave %d: Starting constraint propagation for grid %d, component type %d\n",
						rank, current_grid_idx, current_component_type);

					int iteration = 0;
					int max_iterations = 10;

					do {
						int local_changes = 0;
						iteration++;

						printf("Slave %d: Starting iteration %d for grid %d\n", rank, iteration, current_grid_idx);

						/* Pass the correct already_propagated depth and range (0 to n for full grid scan) */
						int **current_already_propagated_matrix = (max_depth_for_prop > 0 && already_propagated) ? already_propagated[0] : NULL;

						/* Each iteration, we reset the already_propagated matrix */
						if (current_already_propagated_matrix) {
								for (rr = 0; rr < n; ++rr) {
									memset(current_already_propagated_matrix[rr], 0, n * sizeof(int));
								}
							}

						switch (current_component_type) {
						case 0: /* Row constraint propagation */
							local_changes += parallel_naked_candidates_rows(slave_extended_grid, n, current_already_propagated_matrix, 1, 0, n); /* Using depth 1 as a starting point */
							break;
						case 1: /* Column constraint propagation */
							local_changes += parallel_naked_candidates_cols(slave_extended_grid, n, current_already_propagated_matrix, 1, 0, n); /* Using depth 1 */
							break;
						case 2: /* Box constraint propagation */
							local_changes += parallel_naked_candidates_boxes(slave_extended_grid, n, current_already_propagated_matrix, 1, 0, n); /* Using depth 1 */
							break;
						default:
							printf("Slave %d: Unknown component type %d\n", rank, current_component_type);
							break;
						}

                				/* Apply Hidden Singles after Naked Candidates for this iteration */
                				local_changes += parallel_hidden_singles(slave_extended_grid, n);

						printf("Slave %d: Made %d local changes in iteration %d\n", rank, local_changes, iteration);
						changes_made += local_changes; /* Sum up changes across iterations for this task */

						if (local_changes == 0 || iteration >= max_iterations) {
							printf("Slave %d: Stopping iterations - local_changes: %d, iteration: %d\n",
								rank, local_changes, iteration);
							break;
						}

					} while (iteration < max_iterations);

					printf("Slave %d: Constraint propagation completed after %d iterations, total changes: %d\n",
						rank, iteration, changes_made);
				}

				if (already_propagated) {
					for (depth_idx = 0; depth_idx < max_depth_for_prop; depth_idx++) {
						if (already_propagated[depth_idx]) {
							for (j = 0; j < n; j++) {
								if (already_propagated[depth_idx][j]) free(already_propagated[depth_idx][j]);
							}
							free(already_propagated[depth_idx]);
						}
					}
					free(already_propagated);
				}
			} /* End of slave_extended_grid processing block */

			if (changes_made > 0) {
				printf("Slave %d: Made %d changes to grid %d (component type %d)\n",
				rank, changes_made, current_grid_idx, current_component_type);
			} else {
				printf("Slave %d: No changes made to grid %d (component type %d)\n",
				rank, current_grid_idx, current_component_type);
			}

			/* Convert slave's updated extended grid (linked lists) to bitmasks for sending back */
			int** slave_bitmask_grid_to_send = convert_extended_to_bitmask_grid(slave_extended_grid, n);
			if (!slave_bitmask_grid_to_send) {
				fprintf(stderr, "Slave %d: Failed to convert extended grid to bitmask for sending. Sending empty grid.\n", rank);
				changes_made = 0; /* Force no changes flag */
				slave_bitmask_grid_to_send = create_grid(n); /* Send an empty grid to avoid crash */
				if (!slave_bitmask_grid_to_send) {
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				}
			}

			MPI_Send(&current_grid_idx, 1, MPI_INT, 0, 10, MPI_COMM_WORLD);
			MPI_Send(&current_component_type, 1, MPI_INT, 0, 11, MPI_COMM_WORLD);
			MPI_Send(&current_slave_ordinal, 1, MPI_INT, 0, 12, MPI_COMM_WORLD);
			
			/* Now send the actual count of changes made by the slave's propagation */
			MPI_Send(&changes_made, 1, MPI_INT, 0, 13, MPI_COMM_WORLD);

			for (i = 0; i < n; i++) {
				MPI_Send(slave_bitmask_grid_to_send[i], n, MPI_INT, 0, 14 + i, MPI_COMM_WORLD);
			}
			free_grid(slave_bitmask_grid_to_send, n);

			printf("Slave %d: Sent results for grid %d back to master.\n", rank, current_grid_idx);

			MPI_Recv(&current_grid_idx, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
		}

		printf("Slave %d: Received termination signal or no more work. Shutting down.\n", rank);
		if (slave_extended_grid) {
			free_extended_grid(slave_extended_grid, n);
		}
		if (temp_received_bitmask_grid) {
			free_grid(temp_received_bitmask_grid, n);
		}
		printf("Slave %d: Terminating\n", rank);
	}

	MPI_Finalize();
	return 0;
}

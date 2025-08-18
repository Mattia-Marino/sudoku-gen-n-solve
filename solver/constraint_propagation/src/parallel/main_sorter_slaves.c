#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

void swap(int *a, int *b);
void sort(int *a, int n);

/** 
 * @brief Helper function for master to assign a task to a slave.
 *
 * @returns 1 if a task was assigned, 0 otherwise.
 */
int try_assign_task_to_slave(int slave_rank, int **M, int n_cols, int n_rows,
			      int *next_row_idx_to_start_assigning,
			      int *slaves_assigned_to_row_count, /* size n_rows */
			      int *row_is_completed, /* size n_rows */
			      int K_slaves_per_row)
{
	int i; /* Loop variable */
	int row_to_send = -1;
	int slave_ordinal_for_row;

	/* Priority 1: Find a row that has been started but needs more slaves (and not completed) */
	for (i = 0; i < n_rows; ++i) {
		if (!row_is_completed[i] &&
		    slaves_assigned_to_row_count[i] > 0 &&
		    slaves_assigned_to_row_count[i] < K_slaves_per_row) {
			row_to_send = i;
			break;
		}
	}

	/* Priority 2: If no such row, find a new row to start */
	if (row_to_send == -1 && *next_row_idx_to_start_assigning < n_rows) {
		if (!row_is_completed[*next_row_idx_to_start_assigning] &&
		    slaves_assigned_to_row_count
				    [*next_row_idx_to_start_assigning] == 0) {
			row_to_send = *next_row_idx_to_start_assigning;
		}
	}

	if (row_to_send != -1) {
		MPI_Send(&row_to_send, 1, MPI_INT, slave_rank, 0,
			 MPI_COMM_WORLD);
		MPI_Send(M[row_to_send], n_cols, MPI_INT, slave_rank, 1,
			 MPI_COMM_WORLD);
		slaves_assigned_to_row_count[row_to_send]++;
		printf("Master: Assigned row %d to slave %d (this is slave %d/%d for this row).\n",
		       row_to_send, slave_rank,
		       slaves_assigned_to_row_count[row_to_send],
		       K_slaves_per_row);
		
		slave_ordinal_for_row = slaves_assigned_to_row_count[row_to_send];
		MPI_Send(&slave_ordinal_for_row, 1, MPI_INT, slave_rank, 2, MPI_COMM_WORLD);

		/* If this assignment makes the current 'next_row_idx_to_start_assigning' fully assigned, advance the pointer. */
		if (slaves_assigned_to_row_count[row_to_send] ==
			    K_slaves_per_row &&
		    row_to_send == *next_row_idx_to_start_assigning) {
			(*next_row_idx_to_start_assigning)++;
		}
		return 1;
	}
	return 0; /* No task assigned */
}

int main(int argc, char **argv)
{
	int i, j; /* Loop variables */
	int n; /* Matrix size */

	/* Master variables */
	int **M = NULL; /* Matrix size n*n */
	int num_slaves;
	int terminate_signal = -1;
	int row_idx_to_send;
	int sorted_rows_collected;
	int s_rank;
	int *temp_received_row = NULL;
	MPI_Status status;

	/* Slave variables */
	int received_row_idx;
	int *row_buffer = NULL;
	int current_row_idx;
	int slave_ordinal_for_this_row;

	/* MPI variables */
	int rank, size;
	const int K_SLAVES_PER_ROW = 3;

	/* Inizialize MPI */
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* All processes determine n. In this case, it's fixed. */
	n = 9;

	if (rank == 0) { /* Master process */
		if (n == 0) {
			printf("Master: n is 0, no work to do.\n");
			num_slaves = size - 1;
			if (num_slaves > 0)
				for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
					terminate_signal = -1;
					MPI_Send(&terminate_signal, 1, MPI_INT,
						 s_rank, 0, MPI_COMM_WORLD);
				}

		} else {
			/* Initialize matrix */
			M = (int **)malloc(n * sizeof(int *));
			if (M == NULL) {
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
			}

			for (i = 0; i < n; ++i) {
				M[i] = (int *)malloc(n * sizeof(int));
				if (M[i] == NULL) {
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
					return EXIT_FAILURE;
				}
			}

			/* Fill matrix */
			for (i = 0; i < n; ++i)
				for (j = 0; j < n; ++j)
					M[i][j] = (n * (i + 1)) - j;

			/* Print original matrix */
			printf("Original matrix (Master Rank %d):\n", rank);
			for (i = 0; i < n; ++i) {
				for (j = 0; j < n; ++j)
					printf("%d ", M[i][j]);
				printf("\n");
			}
			printf("\n\n");

			printf("Sorting with %d slaves per row...\n",
			       K_SLAVES_PER_ROW);

			num_slaves = size - 1;

			if (num_slaves == 0) {
				printf("No slaves available. Master (Rank %d) will sort all rows.\n",
				       rank);
				for (i = 0; i < n; ++i) {
					sort(M[i], n);
					for (j = 0; j < n; ++j)
						printf("%d ", M[i][j]);
					printf("\n");
				}
			} else if (num_slaves > 0 &&
				   num_slaves < K_SLAVES_PER_ROW) {
				fprintf(stderr,
					"Master: Error - Not enough slaves (%d) to assign %d per row. Need at least %d slaves.\n",
					num_slaves, K_SLAVES_PER_ROW,
					K_SLAVES_PER_ROW);
				for (s_rank = 1; s_rank <= num_slaves;
				     ++s_rank) {
					MPI_Send(&terminate_signal, 1, MPI_INT,
						 s_rank, 0, MPI_COMM_WORLD);
				}
			} else {
				/* Master-slave logic */
				int *slaves_assigned_to_row_count =
					(int *)calloc(n, sizeof(int));
				int *row_is_completed =
					(int *)calloc(n, sizeof(int));
				int next_row_idx_to_start_assigning = 0;
				int total_rows_completed_by_master = 0;

				if (slaves_assigned_to_row_count == NULL ||
				    row_is_completed == NULL) {
					fprintf(stderr,
						"Master: Failed to allocate tracking arrays.\n");
					if (slaves_assigned_to_row_count)
						free(slaves_assigned_to_row_count);
					if (row_is_completed)
						free(row_is_completed);
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
					/* Free M */
					for (i = 0; i < n; ++i)
						free(M[i]);
					free(M);
					return EXIT_FAILURE;
				}

				temp_received_row =
					(int *)malloc(n * sizeof(int));
				if (temp_received_row == NULL) {
					fprintf(stderr,
						"Master: Failed to allocate temp_received_row.\n");
					free(slaves_assigned_to_row_count);
					free(row_is_completed);
					for (i = 0; i < n; ++i)
						free(M[i]);
					free(M);
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
					return EXIT_FAILURE;
				}

				/* Phase 1: Initial dispatch to slaves */
				int slaves_given_work = 0;
				for (s_rank = 1; s_rank <= num_slaves;
				     ++s_rank) {
					int assigned = try_assign_task_to_slave(
						s_rank, M, n, n,
						&next_row_idx_to_start_assigning,
						slaves_assigned_to_row_count,
						row_is_completed,
						K_SLAVES_PER_ROW);
					if (assigned) {
						slaves_given_work++;
					} else {
						/* This slave couldn't be assigned a task initially */
						MPI_Send(&terminate_signal, 1,
							 MPI_INT, s_rank, 0,
							 MPI_COMM_WORLD);
					}
				}

				printf("\n\n");

				/* Phase 2: Collect results and reschedule */
				int slaves_terminated_in_phase2 = 0;
				if (slaves_given_work == 0 && n > 0) { 
					/*
					 * All work done by master or no slaves could be assigned
					 * This case should ideally be handled if num_slaves < K_SLAVES_PER_ROW and n > 0
					 * or if try_assign_task_to_slave fails for all slaves for some reason.
					 * If slaves_given_work is 0, the loop below won't run.
					 * This is generally okay as slaves not given work are already terminated.
					 */
				}

				while (slaves_terminated_in_phase2 < slaves_given_work) {
					MPI_Recv(&received_row_idx, 1, MPI_INT,
						 MPI_ANY_SOURCE, 0,
						 MPI_COMM_WORLD, &status);
					MPI_Recv(temp_received_row, n, MPI_INT,
						 status.MPI_SOURCE, 1,
						 MPI_COMM_WORLD, &status);
					int reporting_slave_rank =
						status.MPI_SOURCE;

					if (!row_is_completed[received_row_idx]) {
						printf("Master: Row %d sorted (by slave %d): ",
						       received_row_idx,
						       reporting_slave_rank);
						for (i = 0; i < n; ++i)
							printf("%d ",
							       temp_received_row
								       [i]);
						printf("\n");
						row_is_completed
							[received_row_idx] = 1;
						total_rows_completed_by_master++;
					}

					/* Try to reschedule the freed slave only if not all unique rows are completed */
					int reassigned = 0;
					if (total_rows_completed_by_master < n) {
						reassigned = try_assign_task_to_slave(
							reporting_slave_rank, M,
							n, n,
							&next_row_idx_to_start_assigning,
							slaves_assigned_to_row_count,
							row_is_completed,
							K_SLAVES_PER_ROW);
					}

					if (!reassigned) {
						/* No more work for this slave or all unique rows are done */
						MPI_Send(&terminate_signal, 1,
							 MPI_INT,
							 reporting_slave_rank,
							 0, MPI_COMM_WORLD);
						slaves_terminated_in_phase2++;
					}
				}

				free(slaves_assigned_to_row_count);
				free(row_is_completed);
				free(temp_received_row);
			}

			/* Free matrix M */
			if (M != NULL) {
				for (i = 0; i < n; ++i)
					if (M[i] != NULL)
						free(M[i]);
				free(M);
				M = NULL;
			}
		}
	} else { /* Slave processes (rank > 0) */
		/* 
		 * Slave logic remains largely the same, but ensure n=0 is handled cleanly
		 * and memory for row_buffer is allocated only if n > 0.
		 */
		MPI_Recv(&current_row_idx, 1, MPI_INT, 0, 0, MPI_COMM_WORLD,
			 &status); /* Initial receive for row_idx or termination */

		if (current_row_idx != terminate_signal &&
		    n > 0) { /* Check n > 0 before allocating */
			row_buffer = (int *)malloc(n * sizeof(int));
			if (row_buffer == NULL) {
				fprintf(stderr,
					"Slave %d: Failed to allocate row_buffer.\n",
					rank);
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE; /* Should not reach here due to Abort */
			}
		}

		while (current_row_idx != terminate_signal) {
			if (n == 0) { /* Should have been caught by terminate_signal, but as a safeguard */
				break;
			}

			MPI_Recv(row_buffer, n, MPI_INT, 0, 1, MPI_COMM_WORLD,
				 &status); /* Tag 1: row data */

			MPI_Recv(&slave_ordinal_for_this_row, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, &status);

			printf("Slave %d: Received row %d as slave %d for this row.\n", rank, current_row_idx, slave_ordinal_for_this_row);
			fflush(stdout);

			sort(row_buffer, n); /* Process the row */

			MPI_Send(
				&current_row_idx, 1, MPI_INT, 0, 0,
				MPI_COMM_WORLD); /* Send back original row index */
			MPI_Send(row_buffer, n, MPI_INT, 0, 1,
				 MPI_COMM_WORLD); /* Send back sorted row */

			/* Wait for next task or termination signal */
			MPI_Recv(&current_row_idx, 1, MPI_INT, 0, 0,
				 MPI_COMM_WORLD, &status);
		}

		if (row_buffer != NULL) {
			free(row_buffer);
			row_buffer = NULL;
		}
	}

	MPI_Finalize();
	return 0;
}

void swap(int *a, int *b)
{
	int t;

	t = *a;
	*a = *b;
	*b = t;
}

void sort(int *a, int n)
{
	int i, j; /* Loop variables */

	for (i = 0; i < n - 1; ++i)
		for (j = i + 1; j < n; ++j)
			if (a[j] < a[i])
				swap(&a[i], &a[j]);
}
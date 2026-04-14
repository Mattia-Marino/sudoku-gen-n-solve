#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
#include <mpi.h>
#include <string.h>

#ifdef USE_MPE
#include <mpe.h>
#endif

#ifdef USE_PAPI

#include <papi.h>
#define MAX_PAPI_EVENTS 8

/* We use global variables to track PAPI events and values across threads */ 
static int g_papi_event_codes[MAX_PAPI_EVENTS];
static const char *g_papi_event_names[MAX_PAPI_EVENTS];
static int g_papi_num_events = 0;
static long long g_papi_rank_totals[MAX_PAPI_EVENTS] = {0};
static pthread_mutex_t g_papi_lock = PTHREAD_MUTEX_INITIALIZER;

const PAPI_hw_info_t *hwinfo = NULL;

static int add_papi_event_if_available(int event_code, const char *event_name)
{
	int probe_set = PAPI_NULL;

	if (g_papi_num_events >= MAX_PAPI_EVENTS) return 0;
	if (PAPI_query_event(event_code) != PAPI_OK) return 0;

	if (PAPI_create_eventset(&probe_set) != PAPI_OK) return 0;
    	if (PAPI_add_event(probe_set, event_code) != PAPI_OK) {
        	PAPI_cleanup_eventset(probe_set);
        	PAPI_destroy_eventset(&probe_set);
        	return 0;
    	}

	g_papi_event_codes[g_papi_num_events] = event_code;
	g_papi_event_names[g_papi_num_events] = event_name;
	g_papi_num_events++;
	return 1;
}

static unsigned long papi_thread_id_fn(void) { 
	return (unsigned long)(uintptr_t)pthread_self(); 
}

static void print_papi_hw_info(int rank)
{
	if (rank != 0) return;
	hwinfo = PAPI_get_hardware_info();

	if(!hwinfo){
		printf("PAPI Hardware Info: Not available\n");
		return;
	}else{
		printf("PAPI Hardware Info:\n");
		printf("  Vendor: %s\n", hwinfo->vendor_string);
		printf("  Model: %s\n", hwinfo->model_string);
		printf("  CPU MHz: %.2f\n", hwinfo->mhz);
		printf("  ncpu: %d\n", hwinfo->ncpu);
		printf("  nnodes: %d\n", hwinfo->nnodes);
		printf("  totalcpus: %d\n", hwinfo->totalcpus);
	}
}

#endif

#include "../../include/pthread_groups.h"
#include "../../include/queue.h"
#include "../../include/sudoku_utils.h"
#include "../../include/debug.h"
#include "../../include/solver.h"

void *outputs[15];

int ***all_grids;	/* All the grids assigned to the computational node */
int sudoku_size;	/* The size of the sudoku */
queue *q_range;		/* The queue containing the operating range for each thread */

typedef void *( *target )( void *);

typedef struct range {
	int start;
	int end;
} range;

/* Worker thread function, solves sudoku puzzles in all_grids in-place */
void *pthreads_solver()
{
	int i;
	range r;

	#ifdef USE_PAPI
		int evset = PAPI_NULL;
		long long vals[MAX_PAPI_EVENTS] ={0};
		PAPI_register_thread();
		if(g_papi_num_events > 0  && PAPI_create_eventset(&evset) == PAPI_OK){
			for(i = 0; i < g_papi_num_events; i++) {
				(void)PAPI_add_event(evset, g_papi_event_codes[i]);
			}
			if(PAPI_start(evset) != PAPI_OK) {
				PAPI_cleanup_eventset(evset);
				PAPI_destroy_eventset(&evset);
				evset = PAPI_NULL;
			}
		}
	#endif

	if (!isEmpty(q_range)) {
		/* Fetch the range */
		dequeue(q_range, &r);
		printf("\tMy range is [%d - %d]\n", r.start, r.end);

		/* Solve the assigned sudoku puzzles in-place */
		for (i = r.start; i <= r.end; ++i)
				sudoku_solver(all_grids[i], sudoku_size);
	}

	#ifdef USE_PAPI
		if (evset != PAPI_NULL) {
			if(PAPI_stop(evset, vals) == PAPI_OK) {;

				/* Aggregate values into global totals with mutex protection */
				pthread_mutex_lock(&g_papi_lock);
				for (i = 0; i < g_papi_num_events; i++) {
					g_papi_rank_totals[i] += vals[i];
				}
				pthread_mutex_unlock(&g_papi_lock);
			}

			PAPI_cleanup_eventset(evset);
			PAPI_destroy_eventset(&evset);
		}
		PAPI_unregister_thread();
	#endif

	return NULL;
}



int main(int argc, char **argv)
{
	/* MPI Variables */
	int rank, size;

	/* Work Distribution */
	long *all_byte_offsets = NULL;	/* Rank 0: byte offsets for each rank */
	int *all_line_counts = NULL;	/* Rank 0: number of lines for each rank */
	long my_byte_offset = 0;	/* This rank's starting byte offset */

	/* Data Buffers */
	char *local_output_buffer = NULL;
	char *final_output_buffer = NULL;

	/* Logic Variables */
	int local_num_lines = 0;
	int total_lines = 0;
	int n_threads = 0;
	int total_threads_requested;
	int local_threads;
	const int MAX_THREADS_PER_PROCESS = 15;

	/* Parsing and Processing */
	char *filename = NULL;
	FILE *file;
	int opt, i;
	target *t_targets;
	struct ThreadGroup *tg;

	/* Post-processing */
	int tot_solved;
	int grid_len;
	char *current_position;

	/* Timing */
	struct timespec start_time, end_time;
	double computation_time;

	/* For debugging purposes */
	struct timespec ts;
	struct timespec ts_start;

	/* MPE Variables */
	#ifdef USE_MPE
		int ev_read_b,   ev_read_e;
		int ev_solve_b,  ev_solve_e;
		int ev_gather_b, ev_gather_e;
	#endif

	/* Initialize MPI */
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	#ifdef USE_MPE
		/* MPE_Init_log(); */
		
		ev_read_b   = MPE_Log_get_event_number();
    		ev_read_e   = MPE_Log_get_event_number();
    		ev_solve_b  = MPE_Log_get_event_number();
    		ev_solve_e  = MPE_Log_get_event_number();
    		ev_gather_b = MPE_Log_get_event_number();
    		ev_gather_e = MPE_Log_get_event_number();

    		MPE_Describe_state(ev_read_b,   ev_read_e,   "Local file read/parse", "blue");
    		MPE_Describe_state(ev_solve_b,  ev_solve_e,  "Local solve",           "green");
    		MPE_Describe_state(ev_gather_b, ev_gather_e, "Gather results",        "red");
	#endif

	#ifdef USE_PAPI
		if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
			if (rank == 0)
				fprintf(stderr, "PAPI init failed\n");
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;	
		}

		if (PAPI_thread_init(papi_thread_id_fn) != PAPI_OK) {
			if (rank == 0)
				fprintf(stderr, "PAPI thread init failed\n");
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;	
		}

		add_papi_event_if_available(PAPI_TOT_CYC, "PAPI_TOT_CYC");
		add_papi_event_if_available(PAPI_TOT_INS, "PAPI_TOT_INS");
		add_papi_event_if_available(PAPI_LD_INS, "PAPI_LD_INS");
		add_papi_event_if_available(PAPI_SR_INS, "PAPI_SR_INS");
		add_papi_event_if_available(PAPI_L1_DCM, "PAPI_L1_DCM");
		add_papi_event_if_available(PAPI_L2_DCM, "PAPI_L2_DCM");
		add_papi_event_if_available(PAPI_L3_TCM, "PAPI_L3_TCM");
		add_papi_event_if_available(PAPI_TLB_DM, "PAPI_TLB_DM");

		if (rank == 0) printf("PAPI enabled: %d events active\n", g_papi_num_events);
		print_papi_hw_info(rank);
	#endif
	

	/* Default sudoku size */
	sudoku_size = 9;

	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	printf("[%3.6f] - Rank %d - Program start\n", 0.0, rank);

	/* ******************************************************************
	* ALL RANKS: ARGUMENT PARSING
	* All MPI processes parse command-line arguments independently,
	* eliminating the need to broadcast configuration values.
	* ******************************************************************/
	{
		int sqrt_n;

		static struct option long_options[] = {
			{"nthreads", required_argument, 0, 'n'},
			{"size",     required_argument, 0, 's'},
			{0, 0, 0, 0}
		};

		while ((opt = getopt_long(argc, argv, "n:s:", long_options, NULL)) != -1) {
			switch (opt) {
			case 'n': n_threads = atoi(optarg); break;
			case 's': sudoku_size = atoi(optarg); break;
			default:
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
			}
		}

		if (optind < argc) {
			filename = argv[optind];
		} else {
			if (rank == 0)
				fprintf(stderr, "Error: Missing filename\n");

			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Basic Validation */
		if (sudoku_size < 1) {
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		sqrt_n = (int)sqrt(sudoku_size);
		if (sqrt_n * sqrt_n != sudoku_size) {
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}
	}

	/* ******************************************************************
	* RANK 0: COMPUTE WORK DISTRIBUTION
	* Instead of reading the entire file and scattering the raw content,
	* rank 0 only counts lines and computes byte offsets so that each
	* rank can independently seek and read its own portion.
	* Work is distributed proportionally to thread count per rank.
	* ******************************************************************/
	if (rank == 0) {
		file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "Error: Unable to open file %s\n", filename);
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Start Timing */
		clock_gettime(CLOCK_MONOTONIC, &start_time);

		/* Count total lines (rewinds file internally) */
		total_lines = count_lines_in_file(file);

		DPRINTF("Rank 0: Found %d lines.\n", total_lines);

		/* First, calculate thread distribution across all ranks */
		int *all_thread_counts = (int *) malloc(size * sizeof(int));
		int total_threads_allocated = 0;
		
		for (i = 0; i < size; i++) {
			int threads_before = i * MAX_THREADS_PER_PROCESS;
			if (threads_before >= n_threads) {
				all_thread_counts[i] = 0;
			} else {
				int remaining = n_threads - threads_before;
				all_thread_counts[i] = (remaining > MAX_THREADS_PER_PROCESS)
									 ? MAX_THREADS_PER_PROCESS : remaining;
			}
			total_threads_allocated += all_thread_counts[i];
		}

		/* Calculate line distribution based on thread counts */
		all_line_counts = (int *) malloc(size * sizeof(int));
		all_byte_offsets = (long *) malloc(size * sizeof(long));

		if (total_threads_allocated > 0) {
			int lines_assigned = 0;
			for (i = 0; i < size; i++) {
				if (i == size - 1) {
					/* Last rank gets remaining lines to handle rounding */
					all_line_counts[i] = total_lines - lines_assigned;
				} else {
					/* Proportional distribution based on thread count */
					all_line_counts[i] = (int)((double)all_thread_counts[i] / 
											   total_threads_allocated * total_lines);
					lines_assigned += all_line_counts[i];
				}
			}
		} else {
			/* No threads allocated - should not happen */
			for (i = 0; i < size; i++)
				all_line_counts[i] = 0;
		}
		
		free(all_thread_counts);

		/* Scan file to compute byte offsets for each rank's start position */
		all_byte_offsets[0] = 0;
		{
			int ch;
			int total_newlines = 0;
			int next_boundary = all_line_counts[0];
			int rank_idx = 1;

			/* Handle case where rank 0 has no lines */
			while (rank_idx < size && all_line_counts[rank_idx - 1] == 0) {
				all_byte_offsets[rank_idx] = 0;
				next_boundary += all_line_counts[rank_idx];
				rank_idx++;
			}

			while (rank_idx < size && (ch = fgetc(file)) != EOF) {
				if (ch == '\n') {
					total_newlines++;
					if (total_newlines == next_boundary) {
						all_byte_offsets[rank_idx] = ftell(file);
						next_boundary += all_line_counts[rank_idx];
						rank_idx++;
						/* Skip ranks with no lines */
						while (rank_idx < size && all_line_counts[rank_idx] == 0) {
							all_byte_offsets[rank_idx] = all_byte_offsets[rank_idx - 1];
							rank_idx++;
						}
					}
				}
			}
		}

		fclose(file);
	}

	/* ******************************************************************
	* DATA DISTRIBUTION
	* Instead of scattering the entire file content (MPI_Scatterv),
	* we scatter only the line counts and byte offsets - a few bytes
	* per rank, rather than megabytes of raw puzzle data.
	* ******************************************************************/
	MPI_Scatter(all_line_counts, 1, MPI_INT,
				&local_num_lines, 1, MPI_INT,
				0, MPI_COMM_WORLD);
	MPI_Scatter(all_byte_offsets, 1, MPI_LONG,
				&my_byte_offset, 1, MPI_LONG,
				0, MPI_COMM_WORLD);

	/* Broadcast total_lines (needed by rank 0 for final verification) */
	MPI_Bcast(&total_lines, 1, MPI_INT, 0, MPI_COMM_WORLD);

	DPRINTF("Rank %d: assigned %d lines at byte offset %ld\n",
			rank, local_num_lines, my_byte_offset);

	/* Calculate local threads based on total requested */
	total_threads_requested = n_threads;
	{
		int threads_before = rank * MAX_THREADS_PER_PROCESS;
		if (threads_before >= total_threads_requested) {
			local_threads = 0;
		} else {
			int remaining = total_threads_requested - threads_before;
			local_threads = (remaining > MAX_THREADS_PER_PROCESS)
						  ? MAX_THREADS_PER_PROCESS : remaining;
		}
	}

	/* ******************************************************************
	* LOCAL I/O AND PROCESSING
	* Each rank opens the file independently, seeks to its byte offset,
	* reads only its assigned lines, then solves with pthreads.
	* This distributes the I/O load across all nodes instead of
	* funneling everything through rank 0.
	* ******************************************************************/

	MPI_Barrier(MPI_COMM_WORLD);	/* Barrier needed to sync, otherwise rank 0 will start processing before the others*/
	clock_gettime(CLOCK_MONOTONIC, &ts);
	computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
	printf("[%3.6f] - Rank %d - All ready\n", computation_time, rank);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
	printf("[%3.6f] - Rank %d - I have %d threads\n", computation_time, rank, local_threads);

	if (local_threads > 0 && local_num_lines > 0) {
		#ifdef USE_MPE
        		MPE_Log_event(ev_read_b, rank, "read_start");
		#endif
		/* Open file and seek to our starting position */
		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Opening file\n", computation_time, rank);

		file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "Rank %d: Error opening file %s\n", rank, filename);
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}
		fseek(file, my_byte_offset, SEEK_SET);

		/* Parse our assigned lines directly into grids */
		all_grids = (int ***) malloc(local_num_lines * sizeof(int **));

		{
			char line_buf[8192];
			for (i = 0; i < local_num_lines; i++) {
				all_grids[i] = create_grid(sudoku_size);
				if (fgets(line_buf, sizeof(line_buf), file) != NULL) {
					/* Remove trailing newline for parser */
					char *nl = strchr(line_buf, '\n');
					if (nl) *nl = '\0';
					if (read_grid_from_string(all_grids[i], line_buf,
											  sudoku_size) != 0) {
						fprintf(stderr,
								"Error parsing grid on rank %d, line %d\n",
								rank, i);
					}
				}
			}
		}
		fclose(file);

		#ifdef USE_MPE
        		MPE_Log_event(ev_read_e, rank, "read_end");
		#endif

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Closing file\n", computation_time, rank);

		q_range = createQueue(sizeof(range));
		{
			range r;
			int load;
			int base_load = local_num_lines / local_threads;
			int rem_load = local_num_lines % local_threads;
			int next_start = 0;

			for (i = 0; i < local_threads; ++i) {
				load = base_load + (i < rem_load ? 1 : 0);

				/* Changed this to avoid overlaps*/
				r.start = next_start;
				r.end = next_start + load - 1;
				next_start += load;

				enqueue(q_range, &r);
			}
		}


		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Starting processing\n", computation_time, rank);

		#ifdef USE_MPE
        		MPE_Log_event(ev_solve_b, rank, "solve_start");
		#endif

		/* Launch Threads */
		t_targets = (target *) malloc(local_threads * sizeof(target));
		for (i = 0; i < local_threads; ++i) t_targets[i] = &pthreads_solver;
		tg = create_thread_group(t_targets, NULL, local_threads);
		join_thread_group(tg, outputs);

		#ifdef USE_MPE
       			MPE_Log_event(ev_solve_e, rank, "solve_end");
		#endif

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Completed processing\n", computation_time, rank);

		/* ******************************************************************
		* PREPARE OUTPUT FOR GATHER
		* ******************************************************************/
		{
			int grid_string_len = get_grid_string_size(sudoku_size);
			int local_output_size_bytes = local_num_lines * grid_string_len;
			char *curr_out_ptr;

			local_output_buffer = (char *) malloc(local_output_size_bytes);
			curr_out_ptr = local_output_buffer;

			for (i = 0; i < local_num_lines; ++i) {
				write_grid_to_string(all_grids[i], curr_out_ptr, sudoku_size);
				curr_out_ptr += grid_string_len;
				free_grid(all_grids[i], sudoku_size);
			}
		}

		free(all_grids);
		free(t_targets);

	} else {
		/* No work for this rank */
		local_output_buffer = (char *) malloc(1);
	}

	#ifdef USE_MPE
    		MPE_Log_event(ev_gather_b, rank, "gather_start");
	#endif

	/* ******************************************************************
	* GATHER RESULTS
	* ******************************************************************/
	{
		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Gathering results\n", computation_time, rank);

		int *recvcounts = NULL;
		int *rdispls = NULL;
		int my_output_bytes;

		if (rank == 0) {
			recvcounts = (int *) malloc(size * sizeof(int));
			rdispls = (int *) malloc(size * sizeof(int));
		}

		/* All ranks calculate how many bytes they generated */
		my_output_bytes = (local_threads > 0 && local_num_lines > 0)
						? (int)(local_num_lines * get_grid_string_size(sudoku_size))
						: 0;

		/* Gather the output sizes to Rank 0 */
		MPI_Gather(&my_output_bytes, 1, MPI_INT,
				   recvcounts, 1, MPI_INT,
				   0, MPI_COMM_WORLD);

		/* Rank 0 calculates displacements for Gatherv */
		if (rank == 0) {
			int current_disp = 0;
			for (i = 0; i < size; i++) {
				rdispls[i] = current_disp;
				current_disp += recvcounts[i];
			}
			final_output_buffer = (char *) malloc(current_disp + 1);
		}

		/* Gather the actual solution strings */
		MPI_Gatherv(local_output_buffer, my_output_bytes, MPI_CHAR,
					final_output_buffer, recvcounts, rdispls, MPI_CHAR,
					0, MPI_COMM_WORLD);

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Gathering completed\n", computation_time, rank);

		/* ******************************************************************
		* FINALIZATION (Rank 0)
		* ******************************************************************/
		if (rank == 0) {
			clock_gettime(CLOCK_MONOTONIC, &end_time);
			computation_time = (end_time.tv_sec - start_time.tv_sec) +
							   (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

			printf("\nTotal computation completed in %.6f seconds.\n",
				   computation_time);

			/* Check solutions */
			tot_solved = 0;
			grid_len = get_grid_string_size(sudoku_size);
			current_position = final_output_buffer;
			for (i = 0; i < total_lines; ++i) {
				if (check_solved_string(current_position, sudoku_size))
					++tot_solved;
				current_position += grid_len;
			}
			printf("Total correctly solved sudoku puzzles: %d\n", tot_solved);

			free(all_line_counts);
			free(all_byte_offsets);
			free(recvcounts);
			free(rdispls);
			free(final_output_buffer);
		}
	}

	#ifdef USE_MPE
    		MPE_Log_event(ev_gather_e, rank, "gather_end");
    		/* MPE_Finish_log("sudoku_mpe"); */
	#endif

	#ifdef USE_PAPI
		long long local_vals[MAX_PAPI_EVENTS] = {0};
		long long global_sums[MAX_PAPI_EVENTS] = {0};
		int sender;

		for (i = 0; i < g_papi_num_events; i++){
			local_vals[i] = g_papi_rank_totals[i];
		}

		if(rank == 0){
			long long tot_cyc = 0, tot_ins = 0, ld_ins = 0, sr_ins = 0;
			long long l1_dcm = 0, l2_dcm = 0, l3_tcm = 0, tlb_dm = 0;
			
			for (i = 0; i < g_papi_num_events; i++){
				global_sums[i] = local_vals[i];
			}

			for (sender = 1; sender < size; sender++) {
				long long recv_vals[MAX_PAPI_EVENTS] = {0};
				MPI_Recv(recv_vals, g_papi_num_events, MPI_LONG_LONG, sender, 777, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				
				for (i = 0; i < g_papi_num_events; i++){
					global_sums[i] += recv_vals[i];
				}
			}

			for (i = 0; i < g_papi_num_events; i++){
				long long avg_rank = (size > 0) ? (global_sums[i] / size) : 0;
            			long long avg_thread = (total_threads_requested > 0)? (global_sums[i] / total_threads_requested): 0;

				printf("PAPI Event %s: Total = %lld\n", g_papi_event_names[i], global_sums[i]);
                		printf("PAPI Event %s: Average per rank = %lld\n", g_papi_event_names[i], avg_rank);
                		printf("PAPI Event %s: Average per thread = %lld\n", g_papi_event_names[i], avg_thread);

				printf("PAPI Event %s: Total = %lld\n", g_papi_event_names[i], global_sums[i]);
				printf("PAPI Event %s: Average per rank = %lld\n", g_papi_event_names[i], global_sums[i] / size);
				printf("PAPI Event %s: Average per thread = %lld\n", g_papi_event_names[i],
					(total_threads_requested > 0) ? (global_sums[i] / total_threads_requested) : 0);

				if (strcmp(g_papi_event_names[i], "PAPI_TOT_CYC") == 0) tot_cyc = global_sums[i];
				else if (strcmp(g_papi_event_names[i], "PAPI_TOT_INS") == 0) tot_ins = global_sums[i];
				else if (strcmp(g_papi_event_names[i], "PAPI_LD_INS") == 0) ld_ins = global_sums[i];
				else if (strcmp(g_papi_event_names[i], "PAPI_SR_INS") == 0) sr_ins = global_sums[i];
				else if (strcmp(g_papi_event_names[i], "PAPI_L1_DCM") == 0) l1_dcm = global_sums[i];
				else if (strcmp(g_papi_event_names[i], "PAPI_L2_DCM") == 0) l2_dcm = global_sums[i];
				else if (strcmp(g_papi_event_names[i], "PAPI_L3_TCM") == 0) l3_tcm = global_sums[i];
				else if (strcmp(g_papi_event_names[i], "PAPI_TLB_DM") == 0) tlb_dm = global_sums[i];
			}

			printf("\nDerived metrics:\n");
			if (tot_cyc > 0 && tot_ins > 0) {
				printf("IPC = %.4f\n", (double)tot_ins / (double)tot_cyc);
				printf("L1 Miss Rate = %.4f%%\n", 100.0 * (double)l1_dcm / (double)tot_ins);
				printf("L2 Miss Rate = %.4f%%\n", 100.0 * (double)l2_dcm / (double)tot_ins);
				printf("L3 Miss Rate = %.4f%%\n", 100.0 * (double)l3_tcm / (double)tot_ins);
				printf("TLB Miss Rate = %.4f%%\n", 100.0 * (double)tlb_dm / (double)tot_ins);
			}
			if (sr_ins > 0) {
				printf("Load/Store Ratio = %.4f\n", (double)ld_ins / (double)sr_ins);
			}
		}else {
            		MPI_Send(local_vals, g_papi_num_events, MPI_LONG_LONG, 0, 777, MPI_COMM_WORLD);
		}

		PAPI_shutdown();
	#endif

	if (local_output_buffer) free(local_output_buffer);

	MPI_Finalize();
	return 0;
}

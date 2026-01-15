#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include <time.h>

#include "../../include/pthread_groups.h"
#include "../../include/queue.h"
#include "../../include/sudoku_utils.h"
#include "../../include/debug.h"
#include "../../include/solver.h"


// void **outputs;
void *outputs[64];

queue *q_in;			/* The queue to fill with sudokus to solve */
queue *q_out;			/* The queue to receive the solved sudokus */
int size;			/* The size of the sudoku , must be a perfect square (4, 9, 16, ...) */

typedef void *( *target )( void *);

struct test_args {
	int num1;
	int num2;
};

// struct solver_args {
// 	queue *in;
// 	queue *out;
// };


void *pthreads_solver()
{
	int **grid;

	/* Process sudokus in batches until the input queue is empty */
	while (!isEmpty(q_in)) {
		/* Try to dequeue a sudoku from the input queue */
		if (isEmpty(q_in))
			break;
		
		dequeue(q_in, &grid);

		/* Call sudoku solver */
		sudoku_solver(grid, size);

		/* Enqueue the solved sudoku */
		enqueue(q_out, &grid);
	}

	// dequeue(q_in, &grid);

	// /* Call sudoku solver */
	// sudoku_solver(grid, size);

	// /* Enqueue the solved sudoku */
	// enqueue(q_out, &grid);

	return NULL;
}

void *test()
{
	printf("Hello, world!\n");

	return NULL;
}

void *test_func(void *arg)
{
	struct test_args *args = (struct test_args*) arg;
	pthread_t self_id = pthread_self();
	
	int sum = args->num1 + args->num2;
	
	printf("I'm executed inside the %lu thread\n", self_id);
	printf("Received numbers %d and %d\n", args->num1, args->num2);
	printf("Sum: %d\n", sum);
	
	return arg;
}

// void *test_func2(void *arg)
// {
// 	DPRINTF("I'm executed inside the %lu thread\n", pthread_self());

// 	DPRINT_SUDOKU(grid, n);
// 	DPRINTF("\n\n\n");
	
// 	return NULL;
// }

// void *test_func2()
// {
// 	int sleep_time = (rand() % 5) + 1;

// 	DPRINTF("Thread %lu sleeping for %d seconds...\n", pthread_self(), sleep_time);
	
// 	sleep(sleep_time);

// 	DPRINTF("I'm executed inside the %lu thread\n", pthread_self());

// 	DPRINT_SUDOKU(grid, n);
// 	DPRINTF("\n\n\n");
	
// 	return NULL;
// }

// void *test_func3(void *arg)
// {
// 	int **grid;

// 	dequeue(q, &grid);

// 	int sleep_time = (rand() % 5) + 1;

// 	DPRINTF("Thread %lu sleeping for %d seconds...\n", pthread_self(), sleep_time);
	
// 	sleep(sleep_time);

// 	DPRINTF("I'm executed inside the %lu thread\n", pthread_self());

// 	DPRINT_SUDOKU(grid, n);
// 	DPRINTF("\n\n\n");
	
// 	return NULL;
// }


int main(int argc, char **argv)
{
	int **grid;			/* The representation of the current sudoku */
	int sqrt_n;			/* The square root of n, needed for checks and cached for performance */
	char *filename;			/* The name of the file to read as input */
	FILE *file;			/* The file given in input */
	
	int read_status;

	target *t_targets;
	struct ThreadGroup *tg;

	int opt;
	int n_threads;			/* Number of threads to be activated in a single group */

	int i;
	int tot_solved;

	struct timespec start_time;
	struct timespec end_time;
	double computation_time;

	
/* ********** PREPARATORY PHASE ********** */

	/* Default values for n_threads and n */
	n_threads = 3;
	size = 9;

	static struct option long_options[] = {
		{"nthreads", required_argument, 0, 'n'},	/* Long option: --nthreads, short: -n */
		{"size",     required_argument, 0, 's'},	/* Long option: --size, short: -s */
		{0, 0, 0, 0}
	};

	/* "n:s:" means both 'n' and 's' require an argument */
	while ((opt = getopt_long(argc, argv, "n:s:", long_options, NULL)) != -1) {
		switch (opt) {
			case 'n':
				n_threads = atoi(optarg);
				break;
			case 's':
				size = atoi(optarg);
				break;
			default:
				exit(EXIT_FAILURE);
		}
	}

	/* After parsing flags, optind is the index of the next element in argv */
	if (optind < argc) {
		filename = argv[optind];
		printf("File name: %s\n", argv[optind]);
	} else {
		fprintf(stderr, "Error: Missing filename\n");
		return 1;
	}

	/* Debug: print given args */
	// DPRINTF("Given args:\n");
	// DPRINTF("- N. threads: %d\n", n_threads);
	// DPRINTF("- Size: %d\n", size);
	// DPRINTF("- File name: %s\n\n", filename);
	printf("Given args:\n");
	printf("- N. threads: %d\n", n_threads);
	printf("- Size: %d\n", size);
	printf("- File name: %s\n\n", filename);

	// *outputs = (void *) malloc(n_threads * sizeof(void *));

	/* Checks on puzzle size */
	if (size < 1) {
		fprintf(stderr, "Error: Puzzle size can't be 0 or a negative number\n");
		return 1;
	}

	sqrt_n = (int)sqrt(size);
	if (sqrt_n * sqrt_n != size) {
		fprintf(stderr, "Error: Size %d must be a perfect square (4, 9, 16, ...)\n", size);
		return 1;
	}

	/* Open the file */
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file %s\n", filename);
		return 1;
	}


/* ********** CONTROLLER PHASE ********** */

	/* Create queues */
	q_in = createQueue(sizeof(grid));
	q_out = createQueue(sizeof(grid));

	/* Start threadgroup */
	t_targets = (target *) malloc(n_threads * sizeof(target));
	for (i = 0; i < n_threads; ++i)
		t_targets[i] = &pthreads_solver;

	tg = create_thread_group(t_targets, NULL, n_threads);

	/* Start timing the computation */
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	/* Read all the given file and enqueue every sudoku */
	while (1) {
		/* Allocate memory for the Sudoku grid */
		grid = create_grid(size);
		if (grid == NULL) {
			fprintf(stderr, "Error: Failed to allocate memory for grid\n");
			fclose(file);
			return 1;
		}

		/* Read the Sudoku grid from the file */
		read_status = read_grid_from_file(grid, file, size);

		if (read_status != 0) {
			/* If read failed because we reached EOF */
			if (feof(file)) {
				DPRINTF("Reached EOF\n");
				break;
			} else {
				fprintf(stderr, "Error: Failed to read grid from file\n");
	
				free_grid(grid, size);
				fclose(file);
				
				return 1;
			}
		}

		/* Enqueue the sudoku */
		enqueue(q_in, &grid);
	}

	join_thread_group(tg, outputs);

	while (!isEmpty(q_in)) {
		tg = create_thread_group(t_targets, NULL, n_threads);
		join_thread_group(tg, outputs);
	}


	// printf("\n\nSudoku puzzles in q_out: %d\n\n", getSize(q_out));

	i = 0;
	tot_solved = 0;
	while (!isEmpty(q_out)) {
		dequeue(q_out, &grid);

		DPRINTF("Sudoku n. %d:\n", i + 1);
		DPRINT_SUDOKU(grid, size);
		DPRINTF("\n\n\n");

		if (check_solved(grid, size))
			++tot_solved;

		/* Free the grid after printing */
		free_grid(grid, size);

		++i;
	}

	/* End timing */
	clock_gettime(CLOCK_MONOTONIC, &end_time);
	computation_time = (end_time.tv_sec - start_time.tv_sec) +
	                   (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
	printf("\nTotal computation completed in %.6f seconds.\n",
	       computation_time);

	printf("Sudoku puzzles completely solved: %d\n", tot_solved);

	DPRINTF("Destroying queues now...\n");
	// destroyQueue(&q_in);
	// destroyQueue(&q_out);
	DPRINTF("Done\n\n");



/* ********** THREAD-GROUP PHASE ********** */

	// struct test_args args[3] = {
	// 	{.num1 = 5, .num2 = 10},
	// 	{.num1 = 15, .num2 = 25},
	// 	{.num1 = 100, .num2 = 200}
	// };
	// void *my_target_args[3] = {&args[0], &args[1], &args[2]};

	// t_targets[0] = &test_func;
	// t_targets[1] = &test_func;
	// t_targets[2] = &test_func;

	// tg = create_thread_group(t_targets, my_target_args, 3);

	// join_thread_group(tg, outputs);





	// t_targets[0] = &test_func2;
	// t_targets[1] = &test_func2;
	// t_targets[2] = &test_func2;

	// tg = create_thread_group(t_targets, NULL, 3);
	// join_thread_group(tg, outputs);




	// q = createQueue(sizeof(grid));
	// enqueue(q, &grid);

	// // t_targets[0] = &test_func3;
	// // t_targets[1] = &test_func3;
	// // t_targets[2] = &test_func3;

	// tg = create_thread_group(t_targets, NULL, 3);
	// join_thread_group(tg, outputs);

	// destroyQueue(&q);









	// t_targets[0] = &test;
	// t_targets[1] = &test;
	// t_targets[2] = &test;

	// tg = create_thread_group(t_targets, NULL, 3);

	// int *my_args[3] = {(int*) 11, (int*) 24, (int*) 63};
	// void **my_target_args = (void**) my_args;

	// t_targets[0] = &test_func;
	// t_targets[1] = &test_func;
	// t_targets[2] = &test_func;

	// struct ThreadGroup *tg = create_thread_group(t_targets, my_target_args, 3);

	// size = tg->size;
	// printf("Thread-group size: %u\n", size);

	// join_thread_group(tg, outputs);

	return 0;
}
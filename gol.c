
#include <pthread.h>
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "colors.h"

#define OUTPUT_NONE (0)
#define OUTPUT_TEXT (1)
#define OUTPUT_VISI (2)
#define PRT_ROW_WISE (0)
#define PRT_COL_WISE (1)

#define ROUND_DELAY (50000)

#define NUM_COLOR_CHOICES (10)

/* For counting the number of live cells in each round. */
static int live = 0;

/* These values need to be shared by all the threads for visualization.
* You do NOT need to worry about synchronized access to these, the graphics
* library will take care of that for you. */
static visi_handle handle;
static color3 *image_buf;
static char *visi_name = "Parallel GOL";


color3 color_choices[10] = {
	{ .r = 255, .g = 0, .b = 0 },
	{ .r = 255, .g = 128, .b = 0 },
	{ .r = 255, .g = 255, .b = 0 },
	{ .r = 128, .g = 255, .b = 0 },
	{ .r = 0, .g = 255, .b = 0 },
	{ .r = 0, .g = 255, .b = 255 },
	{ .r = 0, .g = 128, .b = 255 },
	{ .r = 0, .g = 0, .b = 255 },
	{ .r = 128, .g = 0, .b = 255},
	{ .r = 255, .g = 0, .b = 128},
};

/* This struct represents all the data you need to keep track of your GOL
* simulation.  Rather than passing individual arguments into each function,
* we'll pass in everything in just one of these structs.
*
* NOTE: You need to use the provided fields here, but you'll also need to
* add some of your own. */
struct gol_data {
	/* The number of rows on your GOL game board. */
	int rows;

	/* The number of columns on your GOL game board. */
	int cols;

	/* The number of iterations to run your GOL simulation. */
	int iters;

	/* Which form of output we're generating:
	* 0: only print the first and last board (good for timing).
	* 1: print the board to the terminal at each round.
	* 2: draw the board using the visualization library. */
	int output_mode;

	/* TODO: Use this as an ID to give each thread a unique number. */
	int id;

	// TODO: add other GOL data that you need here (e.g., game board memory).
	int* board;
	int* board_future;

	// TODO: add partitioning information here
	int prt_direction;
	// int prt_minor_length;
	// int prt_start;

	int prt_col_start;
	int prt_col_end;
	int prt_row_start;
	int prt_row_end;

	// TODO: add any other variables you need each thread to have
	pthread_barrier_t* barrier;
	pthread_mutex_t* mutex;
	pthread_t thread;

	int num_threads;

	color3 color;
};


/* Returns the index at which the cell is stored in the board array */
int get_cell_index(struct gol_data* data, int col, int row) {
	// The following two lines make the coordinates wrap around the board.
	col = ((col % data->cols) + data->cols) % data->cols;
	row = ((row % data->rows) + data->rows) % data->rows;

	return (row * data->cols) + col;
}

/* Returns the status of the cell at the given column and row.
* Returns 0 if it is dead and 1 if it is alive */
int get_cell(struct gol_data* data, int col, int row) {
	int index = get_cell_index(data, col, row);
	return data->board[index];
}

/* Returns the future of the cell at the given column and row.
* Returns 0 if it is dead and 1 if it is alive */
int get_cell_future(struct gol_data* data, int col, int row) {
	int index = get_cell_index(data, col, row);
	return data->board_future[index];
}

/* Sets the value of the cell at the given column and row. If value is 1,
* it will be alive. If value is 0, it will be dead. */
void set_cell(struct gol_data* data, int col, int row, int value) {
	int index = get_cell_index(data, col, row);
	data->board[index] = value;
}

/* Sets the future of the cell at the given column and row. If value is 1,
* it will be alive. If value is 0, it will be dead. */
void set_cell_future(struct gol_data* data, int col, int row, int value) {
	int index = get_cell_index(data, col, row);
	data->board_future[index] = value;
}

/* Gets the number of living cells adjacent to the cell at the given row and
* column */
int get_num_cell_neighbors(struct gol_data* data, int col, int row) {
	int ret = 0;

	for(int dx = -1; dx <= 1; dx++) {
		for(int dy = -1; dy <= 1; dy++) {
			if(dx == 0 && dy == 0) continue;

			ret += get_cell(data, col + dx, row + dy);
		}
	}
	return ret;
}

/* Print the board to the terminal.
*
* You can add extra printfs if you'd like, but please leave these fprintf
* calls as they are to make grading easier! */
void print_board(struct gol_data *data, int round) {
	/* Print the round number. */
	fprintf(stderr, "Round: %d\n", round);

	for(int i = 0; i < data->rows; ++i) {
		for(int j = 0; j < data->cols; ++j) {
			//TODO: if cell is alive
			if(get_cell(data, j, i) == 1){
				fprintf(stderr, " @");
			} else{
				fprintf(stderr, " _");
			}
		}
		fprintf(stderr, "\n");
	}

	/* Print the total number of live cells. */
	fprintf(stderr, "Live cells: %d\n", live);

	/* Add some blank space between rounds. */
	fprintf(stderr, "\n\n");
}

/* TODO: copy in your gol_step and other helper functions that implement core
* game logic.  Most of the important logic (e.g., counting live neighbors and
* setting cells to live/dead) doesn't need to change.
*
* NOTE 1: when doing visualization, it's helpful to have each thread use a
* different color.  You can use the 'colors' array defined to assign each
* thread a different color, e.g., buff[buff_index] = colors[data->id];
*
* NOTE 2: you'll need to adjust your loops to ensure that each thread is only
* iterating over its assigned rows and columns rather than the whole board. */

color3 get_dead_color(color3 col) {
	return (color3) {
		.r = col.r / 4,
		.g = col.g / 4,
		.b = col.b / 4
	};
}

/* Perform one round of the GOL simulation.
*
* buff is only used for the visualization library and will be NULL when the
* output_mode is anything other than OUTPUT_VISI.
*
* DO NOT change the declaration of this function.  All data you need to pass
* in should be encapsulated in the gol_data struct. */
void gol_step(color3 *buff, struct gol_data* data) {
	// int colLimit = (data->prt_direction == PRT_ROW_WISE)? data->cols : data->prt_minor_length;
	// int rowLimit = (data->prt_direction == PRT_ROW_WISE)? data->prt_minor_length : data->rows;
	// int colStart = (data->prt_direction == PRT_ROW_WISE)? 0 : data->prt_start;
	// int rowStart = (data->prt_direction == PRT_ROW_WISE)? data->prt_start : 0;

	int local_lives = 0;

	// set futures
	for(int col = data->prt_col_start; col < data->prt_col_end; col++) {
		for(int row = data->prt_row_start; row < data->prt_row_end; row++) {

			int livingNeighbors = get_num_cell_neighbors(data, col, row);
		//	fprintf(stderr, "LivingNeighbors: %d\n", livingNeighbors);

			if (livingNeighbors <= 1){
				set_cell_future(data, col, row, 0);
			} else if(livingNeighbors >= 4){
				set_cell_future(data, col, row, 0);
			} else if(livingNeighbors == 3){
				set_cell_future(data, col, row, 1);
			} else {
				set_cell_future(data, col, row, get_cell(data, col, row));
			}
		}
	}

	pthread_barrier_wait(data->barrier);

	for(int col = data->prt_col_start; col < data->prt_col_end; col++) {
		for(int row = data->prt_row_start; row < data->prt_row_end; row++) {
			int cellFuture = get_cell_future(data, col, row);
			if(cellFuture) local_lives++;

			set_cell(data, col, row, cellFuture);

			/* When using visualization, also update the graphical board. */
			if (buff != NULL) {
				/* Convert row/column number to an index into the
				* visualization grid.  The graphics library uses coordinates
				* that place the origin at a different location from your
				* GOL board. You shouldn't need to change this. */
				int buff_index = (data->rows - (row + 1)) * data->cols + col;

				// TODO: if the cell should be displayed as alive
				if(get_cell(data, col, row) == 1){
					buff[buff_index].r = data->color.r;
					buff[buff_index].g = data->color.g;
					buff[buff_index].b = data->color.b;
				} else{  //not alive
					color3 col = get_dead_color(data->color);
					buff[buff_index].r = col.r;
					buff[buff_index].g = col.g;
					buff[buff_index].b = col.b;
				}
			}
		}
	}
    //mutex
	pthread_mutex_lock(data->mutex);
	live += local_lives;
	pthread_mutex_unlock(data->mutex);
}



void *worker(void *datastruct) {
	// TODO: use type casting to get a struct gol_data * from the void * input.
	struct gol_data* data = (struct gol_data*) datastruct;
	int round;

	for(round = 0; round < data->iters; round++) {
		// 1) if the output mode is OUTPUT_TEXT, only one thread should print the
		// board.

		pthread_barrier_wait(data->barrier);

		if(data->id == 0) {
			if(data->output_mode == OUTPUT_TEXT) {
				system("clear");
				print_board(data, round);
			}
			live = 0;
		}

		pthread_barrier_wait(data->barrier);

		// 2) with appropriate synchronization, execute the core logic of the round
		// (e.g., by calling gol_step)

		gol_step(image_buf, data);

		// 3) if the output mode is > 0, usleep() for a short time to slow down the
		// speed of the animations.

		if(data->output_mode != OUTPUT_NONE) {
			usleep(ROUND_DELAY);
		}

		// 4) if output mode is OUTPUT_VISI, call draw_ready(handle)
		if(data->output_mode == OUTPUT_VISI) {
			draw_ready(handle);
		}
	}

	// TODO: After the final round, one thread should print the final board state

	if(data->id == 0) {
		print_board(data, round);
	}

	/* You're not expecting the workers to return anything important. */
	return NULL;
}


void read_till_end_of_line(FILE* fin) {
	while(fgetc(fin) != '\n');
}

int read_me(struct gol_data *data, char *filename){
	FILE* input_file = fopen(filename, "r");
    //deal with input
	if(input_file == NULL) {
		return 1;
	}

	if(fscanf(input_file, "%d", &(data->rows)) != 1) {
		fclose(input_file);
		return 1;
	}
	read_till_end_of_line(input_file);
	if(fscanf(input_file, "%d", &(data->cols)) != 1) {
		fclose(input_file);
		return 1;
	}
	read_till_end_of_line(input_file);
    //notify if error
	data->board = malloc(data->rows * data->cols * sizeof(int));
	data->board_future = malloc(data->rows * data->cols * sizeof(int));
	for(int i = 0; i < data->rows * data->cols; i++) {
		data->board[i] = 0;
		data->board_future[i] = 0;
	}
	if(fscanf(input_file, "%d", &(data->iters)) != 1) {
		fclose(input_file);
		free(data->board);
		free(data->board_future);
		return 1;
	}
	read_till_end_of_line(input_file);
	int num_pairs;
	int cell_x;
	int cell_y;

	live = 0;

	if(fscanf(input_file, "%d", &(num_pairs)) != 1) {
		free(data->board);
		free(data->board_future);
		fclose(input_file);
	}
	for(int i = 0; i < num_pairs; i++){
		read_till_end_of_line(input_file);
		if(fscanf(input_file, "%d %d", &(cell_x), &(cell_y)) != 2) {
			free(data->board);
			free(data->board_future);
			fclose(input_file);
			return 1;
		}

		if(get_cell(data, cell_x, cell_y) == 0) {
			live++;
		}

		set_cell(data, cell_x, cell_y, 1);
	}
    //close input file
	fclose(input_file);

	return 0;
}



int main(int argc, char *argv[]) {
	//TODO declare main's local variables
	int output_mode;
	struct gol_data data;
	double secs = 0.0;


	if (argc != 6) {
		printf("Wrong number of arguments.\n");
		printf("Usage: %s <input file> <0|1|2> <num threads> <partition> <print_partition>\n", argv[0]);
		return 1;
	}
	output_mode = atoi(argv[2]);

	//TODO: read in the command line arguments, initialize all your variables (board state, synchronization), etc.
	if(read_me(&data, argv[1])) {
		fprintf(stderr, "Error: input file can not be opened or is invalid!\n");
		return 0;
	}

	data.output_mode = atoi(argv[2]);
	data.num_threads = atoi(argv[3]);
	data.prt_direction = atoi(argv[4]);

	/* If we're doing graphics, we need to set up a few things in advance.
	* Other than calling init_pthread_animation, you shouldn't need to change
	* this block.*/
	if (output_mode == OUTPUT_VISI) {
		//TODO: pass the init_pthread_animation function, in order:
		//# of threads, # of rows, # of cols, visi_name (defined above), and the number of iterations.
		handle = init_pthread_animation(data.num_threads, data.rows, data.cols, visi_name, data.iters);
		if (handle == NULL) {
			printf("visi init error\n");
			return 1;
		}

		image_buf = get_animation_buffer(handle);
		if (image_buf == NULL) {
			printf("visi buffer error\n");
			return 1;
		}
	} else {
		handle = NULL;
		image_buf = NULL;
	}

	pthread_mutex_t mutex;
	pthread_barrier_t barrier;

	pthread_mutex_init(&mutex, NULL);
	pthread_barrier_init(&barrier, NULL, data.num_threads);

	data.mutex = &mutex;
	data.barrier = &barrier;

	// TODO: start the clock.
	struct timeval start_time;
	gettimeofday(&start_time, NULL);


	// TODO: set up a gol_data struct for each thread (e.g., with the thread's
	// partition assignment) and create the threads.
	struct gol_data* thread_datas = malloc(sizeof(struct gol_data) * data.num_threads);

	int start_index = 0;

	for(int i = 0; i < data.num_threads; i++) {
		thread_datas[i] = (struct gol_data) {
			.rows = data.rows,
			.cols = data.cols,
			.iters = data.iters,
			.output_mode = data.output_mode,
			.id = i,
			.board = data.board,
			.board_future = data.board_future,
			.prt_direction = data.prt_direction,
			.barrier = data.barrier,
			.mutex = data.mutex,
			.num_threads = data.num_threads,

			.color = color_choices[i % NUM_COLOR_CHOICES]
		};
        //determine data partitioning
		if(data.prt_direction == PRT_ROW_WISE) {
			int width = (data.rows / data.num_threads) + ((i < (data.rows % data.num_threads))? 1 : 0);

			thread_datas[i].prt_col_start = 0;
			thread_datas[i].prt_col_end = data.cols;
			thread_datas[i].prt_row_start = start_index;
			thread_datas[i].prt_row_end = start_index + width;

			start_index += width;
		} else {
			int width = (data.cols / data.num_threads) + ((i < (data.cols % data.num_threads))? 1 : 0);

			thread_datas[i].prt_col_start = start_index;
			thread_datas[i].prt_col_end = start_index + width;
			thread_datas[i].prt_row_start = 0;
			thread_datas[i].prt_row_end = data.rows;

			start_index += width;
		}
        //give information to users if requested
		if(atoi(argv[5])) {
			printf("Thread %d:\tRows: %d-%d\tCols: %d-%d\tTotal: %d\n",
				thread_datas[i].id,
				thread_datas[i].prt_row_start,
				thread_datas[i].prt_row_end,
				thread_datas[i].prt_col_start,
				thread_datas[i].prt_col_end,
				(thread_datas[i].prt_row_end - thread_datas[i].prt_row_start)
					* (thread_datas[i].prt_col_end - thread_datas[i].prt_col_start)
			);
		}

		pthread_create(&thread_datas[i].thread, NULL, worker, (void*) &thread_datas[i]);
	}


	/* If we're doing graphics, call run_animation to tell it how many
	* iterations there will be. */
	if (output_mode == OUTPUT_VISI) {
		//TODO: pass in the number of iterations as the second parameter
		run_animation(handle, data.iters);
	}

	// TODO: join all the threads (that is, wait in this main thread until all
	// the workers are finished.

	for(int i = 0; i < data.num_threads; i++) {
		pthread_join(thread_datas[i].thread, NULL);
	}

	// TODO: stop the clock, print timing results, and clean up memory.
	struct timeval end_time;
	gettimeofday(&end_time, NULL);

	struct timeval delta_time = {
		.tv_sec = end_time.tv_sec - start_time.tv_sec,
		.tv_usec = end_time.tv_usec - start_time.tv_usec
	};

	secs = delta_time.tv_sec + (delta_time.tv_usec/1000000.0);
	fprintf(stderr, "\nTotal time: %0.3f seconds.\n", secs);

	free(data.board);
	free(data.board_future);
	free(thread_datas);

	return 0;
}

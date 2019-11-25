/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include "life.h"
#include "util.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*****************************************************************************
 * Helper function definitions
 ****************************************************************************/

/**
 * Swapping the two boards only involves swapping pointers, not
 * copying values.
 */
#define SWAP_BOARDS( b1, b2 )  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

#define BOARD( __board, __i, __j )  (__board[(__i) + LDA*(__j)])
// #define BOARD_THREAD( __board, __i, __j, _LDA )  (__board[(__i) + (_LDA)*(__j)])

char* game_of_life_threads (char* outboard, 
        char* inboard,
        const int nrows,
        const int ncols,
        const int gens_max);

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/
char*
game_of_life (char* outboard, 
	      char* inboard,
	      const int nrows,
	      const int ncols,
	      const int gens_max)
{
  return game_of_life_threads (outboard, inboard, nrows, ncols, gens_max);
}


int done = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;

typedef struct {
  int nrows;
  int ncols;
  int starting_row;
  int num_thread_rows;
  int gens_max;
  int thread_id;
  char *inboard;
  char *outboard;
} args_t;

void * thread_func(void *vargs) {
    args_t *args = (args_t *)vargs;

    int curgen, i, j;

    //int LDA = args->nrows;
    int nrows = args->nrows;
    int ncols = args->ncols;
    int starting_row = args->starting_row;
    int num_thread_rows = args->num_thread_rows;
    int gens_max = args->gens_max;
    int thread_id = args->thread_id;

    char *inboard = args->inboard;
    char *outboard = args->outboard;

    const int LDA = nrows;

    //printf("[thread %d] num_thread_rows %d ncols %d starting_row %d \n", thread_id, num_thread_rows, ncols, starting_row);

    for (curgen = 0; curgen < gens_max; curgen++)
    {
	   	// memset(outboard + starting_row*ncols, 0, num_thread_rows*ncols);
	    // pthread_barrier_wait(&barrier);

        //printf("[thread %d] curgen %d\n", thread_id, curgen);
	    for (i = starting_row; i < starting_row+num_thread_rows; i++)
	    {
	        for (j = 0; j < ncols; j++)
	        {
	            const int inorth = mod (i-1, nrows);
	            const int isouth = mod (i+1, nrows);
	            const int jwest = mod (j-1, ncols);
	            const int jeast = mod (j+1, ncols);

	            const char neighbor_count = 
                    BOARD (inboard, inorth, jwest) + 
                    BOARD (inboard, inorth, j) + 
                    BOARD (inboard, inorth, jeast) + 
                    BOARD (inboard, i, jwest) +
                    BOARD (inboard, i, jeast) + 
                    BOARD (inboard, isouth, jwest) +
                    BOARD (inboard, isouth, j) + 
                    BOARD (inboard, isouth, jeast);

                BOARD(outboard, i, j) = alivep (neighbor_count, BOARD (inboard, i, j));
	        }

			// if (i == starting_row+1) {
			// 	pthread_barrier_wait(&barrier);
			// }
	    }


	    // signal barrier
	    //printf("thread %d signalling\n", thread_id);
	    pthread_barrier_wait(&barrier);

	    SWAP_BOARDS( outboard, inboard );

	    //printf("barrier passed! thread %d\n", thread_id);
	}

    return NULL;
}

char* game_of_life_threads (char* outboard, 
        char* inboard,
        const int nrows,
        const int ncols,
        const int gens_max)
{
    /* HINT: in the parallel decomposition, LDA may not be equal to
       nrows! */
    //const int LDA = nrows;
    int i;

    //printf("main thread nrows %d ncols %d\n", nrows, ncols, sizeof(inboard[0]));

    // for (i = 0; i < nrows*ncols; i++) {
    //   printf("%d", inboard[i]);
    //   if (i%nrows == nrows-1) printf("\n");
    // }

    int num_threads = 8;
    pthread_t thread_ids[num_threads-1];

    if(pthread_barrier_init(&barrier, NULL, num_threads) != 0) {
    	printf("could not init barrier :(");
    	return NULL;
    }

    // setup args for each thread
    args_t *args = (args_t *)malloc(num_threads*sizeof(args_t *));

    for (i = 0; i < num_threads-1; i++) {
		args[i].nrows = nrows;
		args[i].ncols = ncols;
		args[i].starting_row = i*nrows/num_threads;
		args[i].num_thread_rows = nrows/num_threads;
		args[i].gens_max = gens_max;
		args[i].thread_id = i;

		args[i].inboard = inboard;
		args[i].outboard = outboard;

		//printf("thread id %d: %lu\n", i, thread_ids[i]);
	    int error = pthread_create(&thread_ids[i], NULL, thread_func, (void *)&args[i]);
		if (error != 0) {
			printf("failed to create thread %d\n", i);
			return NULL;
		}
    }

    int this_thread_id = num_threads-1;
	args[this_thread_id].nrows = nrows;
	args[this_thread_id].ncols = ncols;
	args[this_thread_id].starting_row = this_thread_id*nrows/num_threads;
	args[this_thread_id].num_thread_rows = nrows/num_threads;
	args[this_thread_id].gens_max = gens_max;
	args[this_thread_id].thread_id = i;

	args[this_thread_id].inboard = inboard;
	args[this_thread_id].outboard = outboard;
    thread_func(&args[this_thread_id]);

    for(i = 0; i < num_threads-1; i++) {
	   	//printf("thread id %d: %lu\n", i, thread_ids[i]);
	    //pthread_join(thread_ids[i], NULL);
    }

    free(args);
  	pthread_barrier_destroy(&barrier);

    /* 
     * We return the output board, so that we know which one contains
     * the final result (because we've been swapping boards around).
     * Just be careful when you free() the two boards, so that you don't
     * free the same one twice!!! 
     */
    return gens_max % 2 == 1 ? outboard : inboard;
}



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
#define BOARD_THREAD( __board, __i, __j, _LDA )  (__board[(__i) + (_LDA)*(__j)])


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
  return sequential_game_of_life_threads (outboard, inboard, nrows, ncols, gens_max);
}


int done = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

typedef struct {
  int nrows;
  int ncols;
  char *inboard;
  char *outboard;
} args_t;

void * thread_func(void *vargs) {
    args_t *args = (args_t *)vargs;

    int i, j;

    int LDA = args->nrows;
    int nrows = args->nrows;
    int ncols = args->ncols;

    char *inboard = args->inboard;
    char *outboard = args->outboard;

    printf("nrows %d ncols %d\n", nrows, ncols);

    for (i = 0; i < nrows; i++)
    {
        for (j = 0; j < ncols; j++)
        {
            const int inorth = mod (i-1, nrows);
            const int isouth = mod (i+1, nrows);
            const int jwest = mod (j-1, ncols);
            const int jeast = mod (j+1, ncols);

            const char neighbor_count = 
                BOARD_THREAD (inboard, inorth, jwest, LDA) + 
                BOARD_THREAD (inboard, inorth, j, LDA) + 
                BOARD_THREAD (inboard, inorth, jeast, LDA) + 
                BOARD_THREAD (inboard, i, jwest, LDA) +
                BOARD_THREAD (inboard, i, jeast, LDA) + 
                BOARD_THREAD (inboard, isouth, jwest, LDA) +
                BOARD_THREAD (inboard, isouth, j, LDA) + 
                BOARD_THREAD (inboard, isouth, jeast, LDA);

            BOARD_THREAD(outboard, i, j, LDA) = alivep (neighbor_count, BOARD_THREAD (inboard, i, j, LDA));

        }
    }

    pthread_mutex_lock(&lock);

    done++;
    printf( "[thread] done is now %d. Signalling cond.\n", done );

    pthread_cond_signal(&cond); 
    pthread_mutex_unlock(&lock);

    return NULL;
}

char* sequential_game_of_life_threads (char* outboard, 
        char* inboard,
        const int nrows,
        const int ncols,
        const int gens_max)
{
    /* HINT: in the parallel decomposition, LDA may not be equal to
       nrows! */
    //const int LDA = nrows;
    int curgen, i;//, j;

    //printf("main thread nrows %d ncols %d\n", nrows, ncols, sizeof(inboard[0]));

    for (i = 0; i < nrows*ncols; i++) {
      printf("%d", inboard[i]);
      if (i%nrows == nrows-1) printf("\n");
    }

    int num_threads = 4;
    pthread_t thread_ids[num_threads];

    // setup args for each thread
    args_t *args = (args_t *)malloc(num_threads*sizeof(args_t *));

    int thread_nrows = nrows*2/num_threads;
    int thread_ncols = ncols*2/num_threads;

    for (curgen = 0; curgen < gens_max; curgen++)
    {
        printf("curgen %d\n", curgen);
        done = 0;

        for (i = 0; i < num_threads; i++) {
          args[i].nrows = thread_nrows;
          args[i].ncols = thread_ncols;

          // shift inboard and outboard by nrows*i
          args[i].inboard = (char *)malloc(sizeof(char)*thread_nrows*thread_ncols);
          args[i].outboard = (char *)malloc(sizeof(char)*thread_nrows*thread_ncols);

          char * thread_inboard = inboard + i*thread_nrows*thread_ncols;
          memmove(args[i].inboard, thread_inboard, thread_nrows*thread_ncols);

          char * thread_outboard = outboard + i*thread_nrows*thread_ncols;
          memmove(args[i].outboard, thread_outboard, thread_nrows*thread_ncols);
          //args[i].ncols = ncols/num_threads;

          int error = pthread_create(&thread_ids[i], NULL, thread_func, (void *)&args[i]);
          if (error != 0) {
            printf("failed to create thread %d\n", i);
          }
        }

        pthread_mutex_lock(&lock);

        while(done < num_threads) {
            printf( "[thread main] done is %d which is < %d so waiting on cond\n", done, num_threads );
            
            pthread_cond_wait(&cond, &lock); 
            
            //puts( "[thread main] wake - cond was signalled." ); 
        }

        pthread_mutex_unlock(&lock);

        /* HINT: you'll be parallelizing these loop(s) by doing a
           geometric decomposition of the output */
        // for (i = 0; i < nrows; i++)
        // {
        //     for (j = 0; j < ncols; j++)
        //     {
        //         const int inorth = mod (i-1, nrows);
        //         const int isouth = mod (i+1, nrows);
        //         const int jwest = mod (j-1, ncols);
        //         const int jeast = mod (j+1, ncols);

        //         const char neighbor_count = 
        //             BOARD (inboard, inorth, jwest) + 
        //             BOARD (inboard, inorth, j) + 
        //             BOARD (inboard, inorth, jeast) + 
        //             BOARD (inboard, i, jwest) +
        //             BOARD (inboard, i, jeast) + 
        //             BOARD (inboard, isouth, jwest) +
        //             BOARD (inboard, isouth, j) + 
        //             BOARD (inboard, isouth, jeast);

        //         BOARD(outboard, i, j) = alivep (neighbor_count, BOARD (inboard, i, j));

        //     }
        // }
        
        SWAP_BOARDS( outboard, inboard );

        for (i = 0; i < num_threads; i++) {
            memmove(outboard + thread_nrows*thread_ncols*i, args[i].outboard, thread_nrows*thread_ncols);

           free(args[i].inboard);
           free(args[i].outboard);
        }
    }

    free(args);

    for(i = 0; i < num_threads; i++) {
      pthread_join(thread_ids[i], NULL);
    }

    /* 
     * We return the output board, so that we know which one contains
     * the final result (because we've been swapping boards around).
     * Just be careful when you free() the two boards, so that you don't
     * free the same one twice!!! 
     */
    return inboard;
}



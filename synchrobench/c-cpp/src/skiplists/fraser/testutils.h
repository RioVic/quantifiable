#ifndef TESTUTILS_H
#define TESTUTILS_H
#include "intset.h"
#include "stdatomic.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct barrier {
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n);

void barrier_cross(barrier_t *b);

/* 
 * Returns a pseudo-random value in [1; range].
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given program options [r]ange and [i]nitial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
long rand_range(long r);

/* Thread-safe, re-entrant version of rand_range(r) */
long rand_range_re(unsigned int *seed, long r);
#endif

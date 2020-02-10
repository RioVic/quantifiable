#include "testutils.h"

void barrier_init(barrier_t *b, int n)
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

void barrier_cross(barrier_t *b)
{
  pthread_mutex_lock(&b->mutex);
  /* One more thread through */
  b->crossing++;
  /* If not all here, wait */
  if (b->crossing < b->count) {
    pthread_cond_wait(&b->complete, &b->mutex);
  } else {
    pthread_cond_broadcast(&b->complete);
    /* Reset for next time */
    b->crossing = 0;
  }
  pthread_mutex_unlock(&b->mutex);
}

/* 
 * Returns a pseudo-random value in [1; range].
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given program options [r]ange and [i]nitial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
inline long rand_range(long r) {
  int m = RAND_MAX;
  int d, v = 0;
 
  do {
    d = (m > r ? r : m);
    v += 1 + (int)(d * ((double)rand()/((double)(m)+1.0)));
    r -= m;
  } while (r > 0);
  return v;
}
long rand_range(long r);

/* Thread-safe, re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
  int m = RAND_MAX;
  int d, v = 0;
 
  do {
    d = (m > r ? r : m);		
    v += 1 + (int)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
    r -= m;
  } while (r > 0);
  return v;
}
long rand_range_re(unsigned int *seed, long r);

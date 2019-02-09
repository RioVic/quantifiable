#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "delay.h"
#include "queue.h"

#ifndef NUM_OPS
#define NUM_OPS 5000000
#endif

static long nops;
static queue_t * q;
static handle_t ** hds;

void init(int nprocs, int num_ops) {
  /** Use 10^7 as default input size. */
  if (num_ops == 0) nops = num_ops;
  else nops = num_ops

  printf("  Number of operations per thread: %ld\n", nops);

  q = align_malloc(PAGE_SIZE, sizeof(queue_t));
  queue_init(q, nprocs);

  hds = align_malloc(PAGE_SIZE, sizeof(handle_t * [nprocs]));
}

void thread_init(int id, int nprocs) {
  hds[id] = align_malloc(PAGE_SIZE, sizeof(handle_t));
  queue_register(q, hds[id], id);
}

void thread_exit(int id, int nprocs) {
  queue_free(q, hds[id]);
}

void * benchmark(int id, int nprocs, int ratio_enq) {
  void * val = (void *) (intptr_t) (id + 1);
  handle_t * th = hds[id];

  delay_t state;
  delay_init(&state, id);

  struct drand48_data rstate;
  srand48_r(id, &rstate);

  int i;
  for (i = 0; i < nops / nprocs; ++i) {
    long n;
    lrand48_r(&rstate, &n);

    if (n % 100 < ratio_enq)
      enqueue(q, th, val);
    else
      dequeue(q, th);

    delay_exec(&state);
  }

  return val;
}

int verify(int nprocs, void ** results) {
  return 0;
}

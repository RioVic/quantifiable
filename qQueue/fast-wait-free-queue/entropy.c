#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "delay.h"
#include "queue.h"

#ifndef TIMESTAMP
#define TIMESTAMP
#include "timestamp.h"
#endif

#ifndef LOGN_OPS
#define LOGN_OPS 2
#endif

static long nops;
static queue_t * q;
static handle_t ** hds;

long long rdtsc() {
  volatile long long tl;
  asm __volatile__("lfence\nrdtsc" : "=a" (tl): : "%edx"); //lfence is used to wait for prior instruction (optional)
  return tl;
}

void init(int nprocs, int logn) {

  /** Use 10^7 as default input size. */
  if (logn == 0) logn = LOGN_OPS;

  /** Compute the number of ops to perform. */
  nops = 1;
  int i;
  for (i = 0; i < logn; ++i) {
    nops *= 10;
  }

  printf("  Number of operations: %ld\n", nops);

  q = align_malloc(PAGE_SIZE, sizeof(queue_t));
  queue_init(q, nprocs);

  hds = align_malloc(PAGE_SIZE, sizeof(handle_t * [nprocs]));
}

void thread_init(int id, int nprocs) {
  hds[id] = align_malloc(PAGE_SIZE, sizeof(handle_t));
  queue_register(q, hds[id], id);
}

void * benchmark(int id, int nprocs, struct timestamp **ts) {
  void * val = (void *) (intptr_t) (id + 1);
  void * ret;
  handle_t * th = hds[id];

  delay_t state;
  delay_init(&state, id);

  int i;
  for (i = 0; i < nops / nprocs; ++i) {
    unsigned long invoked = rdtsc();
    if (i%2 == 0)
    {
      enqueue(q, th, val);
      strcpy(ts[id][i].type, "Enqueue");
      ts[id][i].val = (intptr_t)val;
      val += nprocs;      
    }
    else
    {
      ret = dequeue(q, th);
      strcpy(ts[id][i].type, "Dequeue");
      ts[id][i].val = (intptr_t)ret;
    }
    
    ts[id][i].key = (id) + (nprocs *i);
    ts[id][i].invoked = invoked;

    delay_exec(&state);
  }

  return val;
}

void * benchmarkIdeal(int id, int nprocs, struct timestamp **ts, struct timestamp *dataIn) {
  void * val = (void *) (intptr_t) (id + 1);
  void * ret;
  handle_t * th = hds[id];

  delay_t state;
  delay_init(&state, id);

  int i;
  for (i = 0; i < nops / nprocs; ++i) {
    unsigned long invoked = rdtsc();
    if (strcmp(dataIn[i].type, "Enqueue") == 0)
    {
      enqueue(q, th, dataIn[i].val);
      strcpy(ts[id][i].type, "Enqueue");
      ts[id][i].val = dataIn[i].val;     
    }
    else if (strcmp(dataIn[i].type, "Dequeue") == 0)
    {
      ret = dequeue(q, th);
      strcpy(ts[id][i].type, "Dequeue");
      ts[id][i].val = (intptr_t)ret;
    }
    else
    {
      printf("Error, type unrecognized\n");
    }
    
    ts[id][i].key = dataIn[i].key;
    ts[id][i].invoked = invoked;

    delay_exec(&state);
  }

  return val;
}

void thread_exit(int id, int nprocs) {
  queue_free(q, hds[id]);
}

#ifdef VERIFY
static int compare(const void * a, const void * b) {
  return *(long *) a - *(long *) b;
}
#endif

int verify(int nprocs, void ** results) {
#ifndef VERIFY
  return 0;
#else
  qsort(results, nprocs, sizeof(void *), compare);

  int i;
  int ret = 0;

  for (i = 0; i < nprocs; ++i) {
    int res = (int) (intptr_t) results[i];
    if (res != i + 1) {
      fprintf(stderr, "expected %d but received %d\n", i + 1, res);
      ret = 1;
    }
  }

  if (ret != 1) fprintf(stdout, "PASSED\n");
  return ret;
#endif
}

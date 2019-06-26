#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "delay.h"
#include "queue.h"

#ifndef LOGN_OPS
#define LOGN_OPS 7
#endif

static long nops;
static queue_t * q;
static handle_t ** hds;

long long **invocations;
long long **returns;
int **enqueues;
int **dequeues;

inline unsigned long rdtsc() {
  volatile unsigned long tl;
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

  //Init invocations and returns
  invocations = malloc(sizeof(long long *) * nprocs);
  returns = malloc(sizeof(long long *) * nprocs);
  dequeues = malloc(sizeof(int *) * nprocs);
  enqueues = malloc(sizeof(int *) * nprocs);

  for (int k = 0; k < nprocs; k++)
  {
    invocations[k] = malloc(sizeof(long long) * nops*2);
    returns[k] = malloc(sizeof(long long) * nops*2);
    dequeues[k] = malloc(sizeof(int) * nops*2);
    enqueues[k] = malloc(sizeof(int) * nops*2);

    for (int j = 0; j < nops*2; j++)
    {
      dequeues[k][j] = -22;
      enqueues[k][j] = -22;
    }
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

void thread_exit(int id, int nprocs) {
  queue_free(q, hds[id]);
}

void exportHistory(int nprocs)
{
  FILE *fp;

  fp = fopen("./dequeueOrder", "w");
  fprintf(fp, "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\n");
  for (int i = 0; i < nops*2; i++)
  {
    for (int k = 0; k < nprocs; k++)
    {
      if (pops[k][i] == -22 && pushes[k][i] == -22)
        continue;

      unsigned long invoked = invocations[k][i];
      unsigned long returned = returns[k][i];
      int val = (pops[k][i] == -22) ? pushes[k][i] : pops[k][i];

      fprintf(fp, "AMD \t QQueue \t %s \t %d \t x \t %d \t %lu \t %lu \n", (pops[k][i] == -22 ? "Push" : "Pop"), k, val, invoked, returned);
    }
  }
  fclose(fp);
}

void * benchmark(int id, int nprocs) {
  void * val = (void *) (intptr_t) (id + 1);
  void * ret;
  handle_t * th = hds[id];

  delay_t state;
  delay_init(&state, id);

  struct drand48_data rstate;
  srand48_r(id, &rstate);

  int i;
  for (i = 0; i < nops / nprocs; ++i) {
    long n;
    lrand48_r(&rstate, &n);
    unsigned long invoked = rdtsc();

    if (n % 2 == 0)
    {
      enqueue(q, th, val);
      enqueues[id][i] = val;
      val += nprocs;
    }
    else
    {
      ret = dequeue(q, th);
      dequeues[id][i] = ret;
    }

    unsigned long returned = rdtsc();

    invocations[id][i] = invoked;
    returned[id][i] = returned;

    delay_exec(&state);
  }

  exportHistory(nprocs);

  return val;
}

int verify(int nprocs, void ** results) {
  return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "delay.h"
#include "queue.h"

#ifndef LOGN_OPS
#define LOGN_OPS 1
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

void exportHistory(int nprocs)
{
  FILE *fp;

  printf("Exporting History\n");
  fp = fopen("./dequeueOrder", "w");
  fprintf(fp, "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\n");
  for (int i = 0; i < nops*2; i++)
  {
    for (int k = 0; k < nprocs; k++)
    {
      if (dequeues[k][i] == -22 && enqueues[k][i] == -22)
        continue;

      unsigned long invoked = invocations[k][i];
      unsigned long returned = returns[k][i];
      int val = (dequeues[k][i] == -22) ? enqueues[k][i] : dequeues[k][i];

      fprintf(fp, "AMD \t QQueue \t %s \t %d \t x \t %d \t %lu \t %lu \n", (dequeues[k][i] == -22 ? "Push" : "Pop"), k, val, invoked, returned);
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

  int i;
  for (i = 0; i < nops / nprocs; ++i) {
    unsigned long invoked = rdtsc();
    if (i%2 == 0)
    {
      enqueue(q, th, val);
      enqueues[id][i] = (int)val;
      val += nprocs;      
    }
    else
    {
      ret = dequeue(q, th);
      dequeues[id][i] = (int)ret;
    }
    unsigned long returned = rdtsc();

    invocations[id][i] = invoked;
    returns[id][i] = returned;

    delay_exec(&state);
  }

  exportHistory(nprocs);

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

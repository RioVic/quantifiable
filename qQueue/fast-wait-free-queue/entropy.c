#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "delay.h"
#include "queue.h"
#include "rdtsc.h"

#ifndef LOGN_OPS
#define LOGN_OPS 2
#endif

struct __attribute__((aligned(64))) timestamp
{
  long long invoked;
  long long returned;
  long long vp; //Visibility Point
  char type[10];
  int val;
  long key;
};

static long nops;
static queue_t * q;
static handle_t ** hds;

struct timestamp **ts;

void init(int nprocs, int logn) {

  /** Use 10^7 as default input size. */
  if (logn == 0) logn = LOGN_OPS;

  /** Compute the number of ops to perform. */
  nops = 1;
  int i;
  for (i = 0; i < logn; ++i) {
    nops *= 10;
  }

  //Init timestamp array
  ts = malloc(sizeof(long long *) * nprocs);

  for (int k = 0; k < nprocs; k++)
  {
    ts[k] = malloc(sizeof(long long) * nops/nprocs);
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

  fp = fopen("./dequeueOrder", "w");
  fprintf(fp, "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tvisibilityPoint\tprimaryStamp\n");
  
  struct timestamp *operation;

  for (int i = 0; i < nprocs; i++)
  {
    for (int k = 0; k < nops/nprocs; k++)
    {
      operation = ts[i];

      if (operation->invoked == -1)
      {
        printf("Error, blank timestamp found\n");
        exit(EXIT_FAILURE);
      }

      fprintf(fp, "AMD \t ALG \t %s \t %d \t x \t %d \t %lli \t %lli \t %lli \n", 
        operation->type, i, operation->val, operation->invoked, operation->vp, (strcmp(operation->type, "Enqueue") ? operation->invoked : operation->vp));
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

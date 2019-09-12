#include <math.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <math.h>
#include "bits.h"
#include "cpumap.h"
#include "benchmark.h"

#ifndef NUM_ITERS
#define NUM_ITERS 1
#endif

#ifndef MAX_PROCS
#define MAX_PROCS 512
#endif

#ifndef MAX_ITERS
#define MAX_ITERS 1
#endif

#ifndef COV_THRESHOLD
#define COV_THRESHOLD 0.02
#endif

struct timestamp **ts;
struct timestamp *dataIn;

static pthread_barrier_t barrier;

char benchName[2048];
long pairwiseSets;
long pairwiseInterval;
long numOpsPerThread;
int isSequential;
int originalNprocs;

struct timestamp *readFile(int numOps, int nprocs)
{
  char fileName[100];
  char procs[8];
  char pInt[8];
  char pSet[8];
  snprintf(procs, 8, "%d\0", originalNprocs);
  snprintf(pInt, 8, "%d\0", pairwiseInterval);
  snprintf(pSet, 8, "%d\0", pairwiseSets);

  strcpy(fileName, "");
  strcat(fileName, benchName+2);
  strcat(fileName, "_");
  strcat(fileName, procs);
  strcat(fileName, "t_");
  strcat(fileName, pInt);
  strcat(fileName, "i_");
  strcat(fileName, pSet);
  strcat(fileName, "_");
  strcat(fileName, "parallel.dat");

  FILE *fp = fopen(fileName, "r");

  if (fp == NULL)
  {
    printf("Erorr: filename not found. Please run the parallel version of this test first to generate a history\n");
    //printf("%s\n", fileName);
    exit(EXIT_FAILURE);
  }

  struct timestamp *dataIn;
  dataIn = malloc(sizeof(struct timestamp) * (numOps));

  char buff[2048];
  char copy[2048];
  char line[255][255];
  char *word;
  int lineIndex = 0, timestampIndex = 0;;

  //Skip first line
  fgets(buff, 2048, (FILE*)fp);

  //Read a line
  while (fgets(buff, 2048, (FILE*)fp) != NULL)
  {
    //printf("%s\n", buff);
    lineIndex = 0;

    strcpy(copy, buff);

    //Seperate the buffer by individual words
    word = strtok(copy, "\t");

    //Store each word of the line at seperate index of array
    while (word != NULL)
    {
      strcpy(line[lineIndex++], word);
      word = strtok(NULL, "\t");
    }

    //printf("%d\n", lineIndex);

    //Copy over the values we need from the line to our dataIn
    strcpy(dataIn[timestampIndex].type, line[2]);
    dataIn[timestampIndex].val = atoi(line[5]);
    dataIn[timestampIndex].invoked = atoll(line[6]);
    dataIn[timestampIndex].key = atol(line[7]);
    timestampIndex++;

    //printf("%s\n", buff);
  }

  fclose(fp);

  return dataIn;
}

void exportHistory(int nprocs)
{
  FILE *fp;
  char fileName[100];
  char procs[8];
  char pInt[8];
  char pSet[8];
  snprintf(procs, 8, "%d\0", originalNprocs);
  snprintf(pInt, 8, "%d\0", pairwiseInterval);
  snprintf(pSet, 8, "%d\0", pairwiseSets);

  strcpy(fileName, "");
  strcat(fileName, benchName+2);
  strcat(fileName, "_");
  strcat(fileName, procs);
  strcat(fileName, "t_");
  strcat(fileName, pInt);
  strcat(fileName, "i_");
  strcat(fileName, pSet);
  strcat(fileName, "_");
  if (isSequential == 0)
    strcat(fileName, "parallel.dat");
  else if (isSequential == 1)
    strcat(fileName, "ideal.dat");

  fp = fopen(fileName, "w");
  fprintf(fp, "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tkey\n");
  
  struct timestamp *operation;

  if (isSequential == 1)
  {
    for (int i = 0; i < numOpsPerThread*originalNprocs; i++)
    {
       operation = &ts[0][i];

      if (operation->invoked == -1)
      {
        printf("Error, blank timestamp found\n");
        exit(EXIT_FAILURE);
      }

      fprintf(fp, "AMD\tALG\t%s\t%d\tx\t%d\t%lli\t%d\n", 
        operation->type, 0, operation->val, operation->invoked, operation->key);
    }
  }
  else
  {
    for (int i = 0; i < nprocs; i++)
    {
      for (int k = 0; k < numOpsPerThread; k++)
      {
        operation = &ts[i][k];

        if (operation->invoked == -1)
        {
          printf("Error, blank timestamp found\n");
          exit(EXIT_FAILURE);
        }

        fprintf(fp, "AMD\tALG\t%s\t%d\tx\t%d\t%lli\t%d\n", 
          operation->type, i, operation->val, operation->invoked, operation->key);
      }
    }
  }

  fclose(fp);
}

static void * thread(void * bits)
{
  int id = bits_hi(bits);
  int nprocs = bits_lo(bits);

  cpu_set_t set;
  CPU_ZERO(&set);

  int cpu = cpumap(id, nprocs);
  CPU_SET(cpu, &set);
  sched_setaffinity(0, sizeof(set), &set);

  thread_init(id, nprocs);
  pthread_barrier_wait(&barrier);

  int i;
  void * result = NULL;

  if (isSequential == 1)
  {
    if (id == 0)
      result = benchmarkIdeal(id, originalNprocs, ts, dataIn);
    pthread_barrier_wait(&barrier);
  }
  else
  {
    //Execute parallel entropy test
    result = benchmark(id, nprocs, ts);
    pthread_barrier_wait(&barrier);
  }

  thread_exit(id, nprocs);
  return result;
}

int main(int argc, const char *argv[])
{
  int nprocs = 0;

  if (argc != 5)
  {
    printf("Please use %s <Number of threads> <Pairwise Invterval> <Pairwise Sets> <Sequential Test: (0:1)>\n", argv[0]);
    return 1;
  }

  nprocs = atoi(argv[1]);
  originalNprocs = nprocs;
  pthread_setconcurrency(nprocs);
  strcpy(benchName, argv[0]);

  pairwiseInterval = atoi(argv[2]);
  pairwiseSets = atoi(argv[3]);
  numOpsPerThread = pairwiseInterval*pairwiseSets;
  isSequential = atoi(argv[4]);

  if (isSequential == 1)
  {
    /** Set entropy ideal case */
    nprocs = 1;
    dataIn = readFile(numOpsPerThread*originalNprocs, originalNprocs);
  }

  pthread_barrier_init(&barrier, NULL, nprocs);
  printf("===========================================\n");
  printf("  Benchmark: %s\n", argv[0]);
  printf("  Number of processors: %d ", originalNprocs);
  if (isSequential)
    printf("(Sequential Test)");
  printf("\n");

  init(originalNprocs, pairwiseInterval, pairwiseSets);

  //Init timestamp array
  ts = malloc(sizeof(struct timestamp *) * originalNprocs);

  for (int k = 0; k < originalNprocs; k++)
  {
    ts[k] = malloc(sizeof(struct timestamp) * numOpsPerThread);
  }

  pthread_t ths[nprocs];
  void * res[nprocs];

  int i;
  for (i = 1; i < nprocs; i++) {
    pthread_create(&ths[i], NULL, thread, bits_join(i, nprocs));
  }

  res[0] = thread(bits_join(0, nprocs));

  for (i = 1; i < nprocs; i++) {
    pthread_join(ths[i], &res[i]);
  }

  exportHistory(nprocs);

  pthread_barrier_destroy(&barrier);
  return verify(nprocs, res);
}


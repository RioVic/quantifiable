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
static double times[MAX_ITERS];
static double means[MAX_ITERS];
static double covs[MAX_ITERS];
static volatile int target;

char benchName[2048];

static size_t elapsed_time(size_t us)
{
  struct timeval t;
  gettimeofday(&t, NULL);
  return t.tv_sec * 1000000 + t.tv_usec - us;
}

static double compute_mean(const double * times)
{
  int i;
  double sum = 0;

  for (i = 0; i < NUM_ITERS; ++i) {
    sum += times[i];
  }

  return sum / NUM_ITERS;
}

static double compute_cov(const double * times, double mean)
{
  double variance = 0;

  int i;
  for (i = 0; i < NUM_ITERS; ++i) {
    variance += (times[i] - mean) * (times[i] - mean);
  }

  variance /= NUM_ITERS;

  double cov = sqrt(variance);;
  cov /= mean;
  return cov;
}

static size_t reduce_min(long val, int id, int nprocs)
{
  static long buffer[MAX_PROCS];

  buffer[id] = val;
  pthread_barrier_wait(&barrier);

  long min = LONG_MAX;
  int i;
  for (i = 0; i < nprocs; ++i) {
    if (buffer[i] < min) min = buffer[i];
  }

  return min;
}

static void report(int id, int nprocs, int i, long us)
{
  long ms = reduce_min(us, id, nprocs);

  if (id == 0) {
    times[i] = ms / 1000.0;
    printf("  #%d elapsed time: %.2f ms\n", i + 1, times[i]);

    if (i + 1 >= NUM_ITERS) {
      int n = i + 1 - NUM_ITERS;

      means[i] = compute_mean(times + n);
      covs[i] = compute_cov(times + n, means[i]);

      if (covs[i] < COV_THRESHOLD) {
        target = i;
      }
    }
  }

  pthread_barrier_wait(&barrier);
}

struct timestamp *readFile(int numOps)
{
  char fileName[100];
  strcpy(fileName, "");
  strcat(fileName, benchName+2);
  strcat(fileName, "OrderParallel.dat");

  FILE *fp = fopen(fileName, "r");
  struct timestamp *dataIn;
  dataIn = malloc(sizeof(struct timestamp) * (numOps));

  char buff[2048];
  char line[8][255];
  char *word;
  int lineIndex = 0, timestampIndex = 0;;

  //Skip first line
  fgets(buff, 2048, (FILE*)fp);

  //Read a line
  while (fgets(buff, 2048, (FILE*)fp) != NULL)
  {
    lineIndex = 0;

    //Seperate the buffer by individual words
    word = strtok(buff, "\t");

    //Store each word of the line at seperate index of array
    while (word != NULL)
    {
      strcpy(line[lineIndex++], word);
      word = strtok(NULL, "\t");
    }

    //Copy over the values we need from the line to our dataIn
    strcpy(dataIn[timestampIndex].type, line[2]);
    dataIn[timestampIndex].val = atoi(line[5]);
    dataIn[timestampIndex].invoked = atoll(line[6]);
    dataIn[timestampIndex].key = atol(line[7]);
    timestampIndex++;
  }

  return dataIn;
}

void exportHistory(int nprocs, int nops)
{
  FILE *fp;
  char fileName[100];
  strcpy(fileName, "");
  strcat(fileName, benchName+2);
  if (nprocs > 1)
    strcat(fileName, "OrderParallel.dat");
  else if (nprocs == 1)
    strcat(fileName, "OrderIdeal.dat");

  fp = fopen(fileName, "w");
  fprintf(fp, "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tkey\n");
  
  struct timestamp *operation;

  for (int i = 0; i < nprocs; i++)
  {
    for (int k = 0; k < nops/nprocs; k++)
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

  if (nprocs == 1)
  {
    result = benchmarkIdeal(id, nprocs, ts, dataIn);
    pthread_barrier_wait(&barrier);
  }
  else
  {
    //Execute parallel entropy test
    for (i = 0; i < MAX_ITERS && target == 0; ++i) {
    long us = elapsed_time(0);
    result = benchmark(id, nprocs, ts);
    pthread_barrier_wait(&barrier);
    us = elapsed_time(us);
    report(id, nprocs, i, us);
   }
  }

  thread_exit(id, nprocs);
  return result;
}

int main(int argc, const char *argv[])
{
  int nprocs = 0;
  int n = 2;

  /** The first argument is nprocs. */
  if (argc > 1) {
    nprocs = atoi(argv[1]);
  }

  /**
   * Use the number of processors online as nprocs if it is not
   * specified.
   */
  if (nprocs == 0) {
    nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  }

  if (nprocs <= 0) return 1;
  else {
    /** Set concurrency level. */
    pthread_setconcurrency(nprocs);
  }

  strcpy(benchName, argv[0]);

  if (nprocs == 1)
  {
    /** Set entropy ideal case */
    dataIn = readFile(pow(10,n));
  }

  /**
   * The second argument is input size n.
   */
  if (argc > 2) {
    n = atoi(argv[2]);
  }

  pthread_barrier_init(&barrier, NULL, nprocs);
  printf("===========================================\n");
  printf("  Benchmark: %s\n", argv[0]);
  printf("  Number of processors: %d\n", nprocs);

  init(nprocs, n);

  //Init timestamp array
  ts = malloc(sizeof(struct timestamp *) * nprocs);

  for (int k = 0; k < nprocs; k++)
  {
    ts[k] = malloc(sizeof(struct timestamp) * pow(10,n));
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

  exportHistory(nprocs, pow(10,n));

  if (nprocs == 1)
    return;

  if (target == 0) {
    target = NUM_ITERS - 1;
    double minCov = covs[target];

    /** Pick the result that has the lowest CoV. */
    int i;
    for (i = NUM_ITERS; i < MAX_ITERS; ++i) {
      if (covs[i] < minCov) {
        minCov = covs[i];
        target = i;
      }
    }
  }

  double mean = means[target];
  double cov = covs[target];
  int i1 = target - NUM_ITERS + 2;
  int i2 = target + 1;

  printf("  Steady-state iterations: %d~%d\n", i1, i2);
  printf("  Coefficient of variation: %.2f\n", cov);
  printf("  Number of measurements: %d\n", NUM_ITERS);
  printf("  Mean of elapsed time: %.2f ms\n", mean);
  printf("===========================================\n");

  char filename[25];
  strcpy(filename, "out/");
  strcat(filename, argv[0]+2);
  strcat(filename, ".dat");
  FILE *fp = fopen(filename, "a+");

  fprintf(fp, "%s\t50-50\t%d\t%.2f\t10000000\n", argv[0]+2, nprocs, mean);

  fclose(fp);

  pthread_barrier_destroy(&barrier);
  return verify(nprocs, res);
}


#ifndef TIMESTAMP
#define TIMESTAMP
#include "timestamp.h"
#endif

#ifndef BENCHMARK_H
#define BENCHMARK_H

extern long long rdtsc();
extern void init(int nprocs, long pInterval, long pSets;);
extern void thread_init(int id, int nprocs);
extern void * benchmark(int id, int nprocs, struct timestamp **);
extern void * benchmarkIdeal(int id, int nprocs, struct timestamp **, struct timestamp *);
extern void thread_exit(int id, int nprocs);
extern int verify(int nprocs, void ** results);

#endif /* end of include guard: BENCHMARK_H */

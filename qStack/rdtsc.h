#ifndef RDTSC_H
#define RDTSC_H

inline long long rdtsc() {
  volatile long long tl;
  asm __volatile__("rdtsc" : "=a" (tl): : "%edx"); //lfence is used to wait for prior instruction (optional)
  return tl;
}

#endif
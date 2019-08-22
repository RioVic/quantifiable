#ifndef RDTSC_H
#define RDTSC_H

inline long long rdtsc() {
  volatile long long low, high;
  asm __volatile__("rdtsc" : "=a" (low), "=d" (high)); //lfence is used to wait for prior instruction (optional)
  return ((long long)high << 32) | low;
}

#endif
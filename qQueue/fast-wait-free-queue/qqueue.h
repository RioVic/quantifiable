#ifndef QQUEUE_H
#define QQUEUE_H

#ifdef QQUEUE
#include "align.h"
#include "hzdptr.h"

typedef struct _node_t {
  struct _node_t * volatile next DOUBLE_CACHE_ALIGNED;
  void * data DOUBLE_CACHE_ALIGNED;
  int op DOUBLE_CACHE_ALIGNED;
} node_t DOUBLE_CACHE_ALIGNED;

typedef struct _queue_t {
  struct _node_t ** volatile head DOUBLE_CACHE_ALIGNED;
  struct _node_t ** volatile tail DOUBLE_CACHE_ALIGNED;
  int *threadIndex;
  int nprocs;
} queue_t DOUBLE_CACHE_ALIGNED;

typedef struct _handle_t {
  hzdptr_t hzd;
  int id;
} handle_t DOUBLE_CACHE_ALIGNED;

#endif

#endif /* end of include guard: QQUEUE_H */

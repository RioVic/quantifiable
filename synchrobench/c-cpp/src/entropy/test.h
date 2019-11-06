#ifndef TEST_H
#define TEST_H
#include "testutils.h"

typedef struct thread_data {
  val_t first;
  long range;
  long unit_tx;
  unsigned long add_operations;
  unsigned long del_operations;
  unsigned long nb_add;
  unsigned long nb_added;
  unsigned long nb_remove;
  unsigned long nb_removed;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned long nb_aborts;
  unsigned long nb_aborts_locked_read;
  unsigned long nb_aborts_locked_write;
  unsigned long nb_aborts_validate_read;
  unsigned long nb_aborts_validate_write;
  unsigned long nb_aborts_validate_commit;
  unsigned long nb_aborts_invalid_memory;
  unsigned long max_retries;
  unsigned int seed;
  intset_l_t *set;
  barrier_t *barrier;
} thread_data_t;

void *test(void *data);
#endif

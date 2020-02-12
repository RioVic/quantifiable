#ifndef TEST_H
#define TEST_H
#include "testutils.h"
#include "options.h"
#include "intset.h"

typedef enum _operation_type {
  ADD,
  DELETE,
  READ
} operation_type;

typedef struct operation {
  int id;
  operation_type type;
  int arg;
  long long unsigned completed_timestamp;
  long long unsigned completed_timestamp_ideal;
  int result;
  int replay_result;
  int thread_id;
} operation_t;

typedef struct thread_data {
  unsigned int first;
  int thread_id;
  long range;
  long unit_tx;
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
  struct sl_set *set;
  barrier_t *barrier;
  operation_t *ops;
  unsigned int num_ops;
} thread_data_t;


void reset_tests();
void *test(void *data);
operation_t *prepare_test(options_t opts, int seed);
void replay(thread_data_t *data);
void dump_operations(operation_t *ops, int num_ops, char* filename, int isIdeal);

#endif

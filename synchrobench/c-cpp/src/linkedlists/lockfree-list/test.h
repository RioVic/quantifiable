#ifndef TEST_H
#define TEST_H
#include "intset.h"

#define IS_IDEAL 1
#define IS_PARALLEL 0

#define DEFAULT_ADD_OPERATIONS 100000
#define DEFAULT_DEL_OPERATIONS 100000
#define DEFAULT_READ_OPERATIONS 200000
#define DEFAULT_ELEMENT_RANGE 256
#define DEFAULT_THREAD_NUM 8
#define DEFAULT_SEED 0 
#define DEFAULT_OUTPUT "ops"

typedef struct options {
	int add_operations;
	int del_operations;
	int read_operations;
	long range;
	int seed;
	int thread_num;
	char filename[1000];
} options_t;

typedef struct barrier {
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
} barrier_t;

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
  val_t first;
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
  intset_t *set;
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

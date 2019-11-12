/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses to the linked list integer set
 *
 * Copyright (c) 2009-2010.
 *
 * test.c is part of Synchrobench
 * 
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define IS_IDEAL 1
#define IS_PARALLEL 0

#include "test.h"
#include "options.h"
#include "string.h"

void create_thread(thread_data_t *data, pthread_t *thread, options_t opt, 
	intset_l_t *set, barrier_t *barrier, operation_t *operations, int thread_id);
void print_stats(thread_data_t *data, int nb_threads, intset_l_t *set);

int main(int argc, char **argv)
{
	
  intset_l_t *set;
  int i, c;
  val_t last = 0; 
  val_t val = 0;

  thread_data_t *data;
  pthread_t *threads;
  barrier_t barrier;

  sigset_t block_set;
  options_t opt = read_options(argc, argv); 
	
  printf("Set type     : lazy linked list\n");
  printf("Thread num   : %d\n", opt.thread_num);
  printf("Value range  : %ld\n", opt.range);
  printf("Seed         : %d\n", opt.seed);
  printf("#add ops     : %d\n", opt.add_operations);
  printf("#delete ops  : %d\n", opt.del_operations);
  printf("#read ops    : %d\n", opt.read_operations);
  printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
	 (int)sizeof(int),
	 (int)sizeof(long),
	 (int)sizeof(void *),
	 (int)sizeof(uintptr_t));
	
	
  if ((data = (thread_data_t *)malloc(opt.thread_num * sizeof(thread_data_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((threads = (pthread_t *)malloc(opt.thread_num * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
	
  if (opt.seed == 0) {
    int seed = (int)time(0);
    printf("using time seed: %d\n", seed);
    srand((int)time(0));
  } else {
    srand(opt.seed);
  }
	
  set = set_new_l();
	
  stop = 0;
	
  /* Init STM */
  printf("Initializing STM\n");
	
  unsigned int seed = rand();
  operation_t *operations = prepare_test(opt, &seed);

  /* Access set from all threads */
  barrier_init(&barrier, opt.thread_num + 1);
  for (i = 0; i < opt.thread_num; i++) {
      printf("Creating thread %d\n", i);
      create_thread(data + i, threads + i, opt, set, &barrier, operations, i);
  }
	
  /* Start threads */
  barrier_cross(&barrier);
	
  printf("STARTING...\n");
	
  /* Wait for thread completion */
  for (i = 0; i < opt.thread_num; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }

  printf("STOPPED...\n");
	
	
  print_stats(data, opt.thread_num, set);

  // replay and output the results
  data[0].set = set_new_l();
  replay(&data[0]);

  char ideal[strlen(opt.filename) + 100];
  char parallel[strlen(opt.filename) + 100];
  strcpy(ideal, opt.filename);
  strcpy(parallel, opt.filename);

  dump_operations(operations, data[0].num_ops, strcat(ideal, "_ideal.dat"), IS_IDEAL);
  dump_operations(operations, data[0].num_ops, strcat(parallel, "_parallel.dat"), IS_PARALLEL);
  /* Delete set */
  set_delete_l(set);
	
  free(threads);
  free(data);
	
  return 0;
}

void print_stats(thread_data_t* data, int nb_threads, intset_l_t *set) {

    unsigned long reads, effreads, updates, effupds, aborts, aborts_locked_read, aborts_locked_write,
    aborts_validate_read, aborts_validate_write, aborts_validate_commit,
    aborts_invalid_memory, max_retries;

  aborts = 0;
  aborts_locked_read = 0;
  aborts_locked_write = 0;
  aborts_validate_read = 0;
  aborts_validate_write = 0;
  aborts_validate_commit = 0;
  aborts_invalid_memory = 0;
  reads = 0;
  effreads = 0;
  updates = 0;
  effupds = 0;
  max_retries = 0;
  int i, size=0;
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #add        : %lu\n", data[i].nb_add);
    printf("    #added    : %lu\n", data[i].nb_added);
    printf("  #remove     : %lu\n", data[i].nb_remove);
    printf("    #removed  : %lu\n", data[i].nb_removed);
    printf("  #contains   : %lu\n", data[i].nb_contains);
    printf("  #found      : %lu\n", data[i].nb_found);
    printf("  #aborts     : %lu\n", data[i].nb_aborts);
    printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
    printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
    printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
    printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
    printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
    printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
    printf("  Max retries : %lu\n", data[i].max_retries);
    aborts += data[i].nb_aborts;
    aborts_locked_read += data[i].nb_aborts_locked_read;
    aborts_locked_write += data[i].nb_aborts_locked_write;
    aborts_validate_read += data[i].nb_aborts_validate_read;
    aborts_validate_write += data[i].nb_aborts_validate_write;
    aborts_validate_commit += data[i].nb_aborts_validate_commit;
    aborts_invalid_memory += data[i].nb_aborts_invalid_memory;
    reads += data[i].nb_contains;
    effreads += data[i].nb_contains + 
      (data[i].nb_add - data[i].nb_added) + 
      (data[i].nb_remove - data[i].nb_removed); 
    updates += (data[i].nb_add + data[i].nb_remove);
    effupds += data[i].nb_removed + data[i].nb_added; 
		
    //size += data[i].diff;
    size += data[i].nb_added - data[i].nb_removed;
    if (max_retries < data[i].max_retries)
      max_retries = data[i].max_retries;
  }
  printf("Set size      : %d (expected: %d)\n", set_size_l(set), size);
  printf("#txs          : %lu\n", reads + updates);
	
  printf("#read txs     : ");
	
  printf("#eff. upd rate: %f \n", 100.0 * effupds / (effupds + effreads));
	
  printf("#update txs   : ");
  printf("%lu\n", updates);
	
  printf("#aborts       : %lu\n", aborts);
  printf("  #lock-r     : %lu\n", aborts_locked_read);
  printf("  #lock-w     : %lu\n", aborts_locked_write);
  printf("  #val-r      : %lu\n", aborts_validate_read);
  printf("  #val-w      : %lu\n", aborts_validate_write);
  printf("  #val-c      : %lu\n", aborts_validate_commit);
  printf("  #inv-mem    : %lu\n", aborts_invalid_memory);
  printf("Max retries   : %lu\n", max_retries);
}


void create_thread(thread_data_t *data, pthread_t *thread, options_t opt, 
	intset_l_t *set, barrier_t *barrier, operation_t *operations, int thread_id) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    data->thread_id = thread_id;
    data->range = opt.range;
    data->unit_tx = DEFAULT_LOCKTYPE;
    data->nb_add = 0;
    data->nb_added = 0;
    data->nb_remove = 0;
    data->nb_removed = 0;
    data->nb_contains = 0;
    data->nb_found = 0;
    data->nb_aborts = 0;
    data->nb_aborts_locked_read = 0;
    data->nb_aborts_locked_write = 0;
    data->nb_aborts_validate_read = 0;
    data->nb_aborts_validate_write = 0;
    data->nb_aborts_validate_commit = 0;
    data->nb_aborts_invalid_memory = 0;
    data->max_retries = 0;
    data->set = set;
    data->barrier = barrier;
    data->ops = operations;
    data->num_ops = opt.add_operations + opt.del_operations + opt.read_operations;
    if (pthread_create(thread, &attr, test, (void *)(data)) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
    pthread_attr_destroy(&attr);
}

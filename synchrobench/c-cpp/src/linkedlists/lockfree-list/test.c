#include "test.h"
#include "stdio.h"
#include "string.h"
#include "stdatomic.h"

options_t read_options(int argc, char** argv) {
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"thread-num",                required_argument, NULL, 't'},
    {"range",                     required_argument, NULL, 'R'},
    {"seed",                      required_argument, NULL, 'S'},
    {"add-operations",            required_argument, NULL, 'a'},
    {"del-operations",            required_argument, NULL, 'd'},
    {"read-operations",           required_argument, NULL, 'r'},
    {"output-prefix",             required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0}
  };

  options_t options;
  memset(&options, 0, sizeof options);
  options.thread_num = DEFAULT_THREAD_NUM;
  options.range = DEFAULT_ELEMENT_RANGE;
  options.seed = DEFAULT_SEED;
  options.add_operations = DEFAULT_ADD_OPERATIONS;
  options.del_operations = DEFAULT_DEL_OPERATIONS;
  strcpy(options.filename, DEFAULT_OUTPUT);

  int i, c;
  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "ht:R:S:a:d:r:o:", long_options, &i);
		
    if(c == -1)
      break;
		
    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;
		
    switch(c) {
    case 0:
      /* Flag is automatically set */
      break;
    case 'h':
      printf("intset - quantifiable entropy test "
	     "(linked list)\n"
	     "\n"
	     "Usage:\n"
	     "  entropy [options...]\n"
	     "\n"
	     "Options:\n"
	     "  -h, --help\n"
	     "        Print this message\n"
	     "  -t, --thread-num <int>\n"
	     "        Number of threads (default=" XSTR(DEFAULT_THREAD_NUM) ")\n"
	     "  -R, --range <int>\n"
	     "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
	     "  -S, --seed <int>\n"
	     "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
	     "  -a, --add-operations <int>\n"
	     "        Total add operations (default=" XSTR(DEFAULT_ADD_OPERATIONS) ")\n"
	     "  -d, --del-operations <int>\n"
	     "        Total delete operations (default=" XSTR(DEFAULT_DEL_OPERATIONS) ")\n"
	     "  -r, --read-operations <int>\n"
	     "        Total read operations (default=" XSTR(DEFAULT_READ_OPERATIONS) ")\n"
	     "  -o, --output-prefix <int>\n"
	     "        output file (default=" DEFAULT_OUTPUT ")\n"
	     );
      exit(0);
    case 'd':
      options.del_operations = atoi(optarg);
      break;
    case 't':
      options.thread_num = atoi(optarg);
      break;
    case 'R':
      options.range = atol(optarg);
      break;
    case 'S':
      options.seed = atoi(optarg);
      break;
    case 'a':
      options.add_operations = atoi(optarg);
      break;
    case 'r':
      options.read_operations = atoi(optarg);
      break;
    case 'o':
      strcpy(options.filename, optarg);
	  break;
    default:
      exit(1);
    }
  }

  assert(options.thread_num >= 0);
  assert(options.range > 0);
  assert(options.add_operations >= 0);
  assert(options.del_operations >= 0);
  assert(options.read_operations >= 0);
  assert(options.add_operations || options.del_operations || options.read_operations);
  return options;
}

void barrier_init(barrier_t *b, int n)
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

void barrier_cross(barrier_t *b)
{
  pthread_mutex_lock(&b->mutex);
  /* One more thread through */
  b->crossing++;
  /* If not all here, wait */
  if (b->crossing < b->count) {
    pthread_cond_wait(&b->complete, &b->mutex);
  } else {
    pthread_cond_broadcast(&b->complete);
    /* Reset for next time */
    b->crossing = 0;
  }
  pthread_mutex_unlock(&b->mutex);
}

/* 
 * Returns a pseudo-random value in [1; range].
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given program options [r]ange and [i]nitial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
inline long rand_range(long r) {
  int m = RAND_MAX;
  int d, v = 0;
 
  do {
    d = (m > r ? r : m);
    v += 1 + (int)(d * ((double)rand()/((double)(m)+1.0)));
    r -= m;
  } while (r > 0);
  return v;
}
long rand_range(long r);

/* Thread-safe, re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
  int m = RAND_MAX;
  int d, v = 0;
 
  do {
    d = (m > r ? r : m);		
    v += 1 + (int)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
    r -= m;
  } while (r > 0);
  return v;
}
long rand_range_re(unsigned int *seed, long r);

void create_thread(thread_data_t *data, pthread_t *thread, options_t opt, 
intset_t *set, barrier_t *barrier, operation_t *operations, int thread_id);
void print_stats(thread_data_t *data, int nb_threads, intset_t *set);

long long current_time_ns (void)
{
    long            ns; // Nanoseconds
    time_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s  = spec.tv_sec;
    ns = spec.tv_nsec; 

    return (long long)s * 1000000000 + ns;
}

int opid = 0;
int thread_num = 0;

void reset_tests() {
    opid = 0;
}

void *test(void *data) {
    thread_data_t *d = (thread_data_t *)data;

    barrier_cross(d->barrier);
	printf("Barrier in thread %d crossed\n", d->thread_id);
    /* Wait on barrier */
	
	int i = 0;
    while (1) {
		int this_op = d->thread_id + (thread_num * i);

		if (this_op > d->num_ops) {
			break;
		}
		
		operation_t *op = &d->ops[this_op];
		op->thread_id = d->thread_id;
		op->completed_timestamp = current_time_ns();

		switch (op->type) {
			case ADD:
			op->result = set_add(d->set, op->arg, TRANSACTIONAL);
			if (op->result) d->nb_added++;
			d->nb_add++;
			break;
			case DELETE:
			op->result = set_remove(d->set, op->arg, TRANSACTIONAL);
			if (op->result) d->nb_removed++;
			d->nb_remove++;
			break;
			case READ:
			op->result = set_contains(d->set, op->arg, TRANSACTIONAL);
			if (op->result) d->nb_found++;
			d->nb_contains++;
			break;
			default:
			assert(0);
		}
        i++;
    } 
			
    return NULL;
}

void replay(thread_data_t *d) {
    int i;
    for (i = 0; i < d->num_ops; i++) {
	operation_t *op = &d->ops[i];

	switch (op->type) {
	    case ADD:
		op->replay_result = set_add(d->set, op->arg, TRANSACTIONAL);
		break;
	    case DELETE:
		op->replay_result = set_remove(d->set, op->arg, TRANSACTIONAL);
		break;
	    case READ:
		op->replay_result = set_contains(d->set, op->arg, TRANSACTIONAL);
		break;
	    default:
		assert(0);
	} 
	op->completed_timestamp_ideal = current_time_ns();
    } 
}

operation_t *prepare_test(options_t opt, int seed) {
    int num_ops = opt.add_operations + opt.del_operations + opt.read_operations;
	// + 100000 to side-step a glibc bug I encountered, instead of fixing my system.
	// https://sourceware.org/bugzilla/show_bug.cgi?id=20116
    operation_t *operations = (operation_t*)malloc((num_ops) * sizeof (operation_t));
    assert(operations);

	thread_num = opt.thread_num;
    
    int i;
    int index = 0;
	int range_arg = 0; //Used to evenly distribute range of values among each operation
    for (i = 0; i < opt.add_operations; i++) {
        operations[index].type = ADD;
		operations[index].arg = range_arg;
		range_arg = (range_arg + 1) % opt.range;
		index++;
    }

	range_arg = 0;
    for (i = 0; i < opt.del_operations; i++) {
        operations[index].type = DELETE;
		operations[index].arg = range_arg;
		range_arg = (range_arg + 1) % opt.range;
		index++;
    }

	range_arg = 0;
    for (i = 0; i < opt.read_operations; i++) {
        operations[index].type = READ;
		operations[index].arg = range_arg;
		range_arg = (range_arg + 1) % opt.range;
		index++;
    }

    // scramble.
    for (i = num_ops-1; i >= 0; i--) {
	int si = (rand()%(i+1));
	operation_t tmp = operations[si];
	operations[si] = operations[i];
	operations[i] = tmp;
    }

    // id them ???
    for (i = 0; i < num_ops; i++) {
        operations[i].id = i;
    }

    return operations;
}

int compare_timestamp(const void *p_a, const void *p_b)
{
	operation_t *a = (operation_t *) p_a;
	operation_t *b = (operation_t *) p_b;
	return (a->completed_timestamp - b->completed_timestamp);
}

void sort_by_timestamp(operation_t *operations, int num_ops)
{
	qsort(operations, num_ops, sizeof(operation_t), compare_timestamp);
}

void dump_operations(operation_t *ops, int num_ops, char* filename, int isIdeal) {
	printf("opening %s to write operations...\n", filename);
    FILE *fp = fopen(filename, "w");
    fprintf(fp, "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\tkey\n");

    int i;
    for (i = 0; i < num_ops; i++) {
	char op_buf[512];
	switch (ops[i].type) {
	    case READ:
		sprintf(op_buf, "%s", "CONTAINS");
		break;
	    case ADD:
		sprintf(op_buf, "%s", "ADD");
		break;
	    case DELETE:
		sprintf(op_buf, "%s", "REMOVE");
		break;
	}

	fprintf(fp, "AMD\tLazyList\t%s\t%d\t%d\t%d\t%llu\t%d\t%d\t%llu\t%d\n",
		op_buf,
		ops[i].thread_id,
		isIdeal ? ops[i].replay_result : ops[i].result,
		ops[i].arg,
		isIdeal ? ops[i].completed_timestamp_ideal : ops[i].completed_timestamp,
		-1,
		-1,
		isIdeal ? ops[i].completed_timestamp_ideal : ops[i].completed_timestamp,
		ops[i].id
	       );
    }
    fclose(fp);
};

int main(int argc, char **argv)
{
	
  intset_t *set;
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

   assert(opt.add_operations == opt.del_operations);
   assert(opt.add_operations % opt.range == 0);
	
	
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
	
  set = set_new();
	
  stop = 0;
	
  /* Init STM */
  printf("Initializing STM\n");
	
  operation_t *operations = prepare_test(opt, (int)opt.seed);

  /* Access set from all threads */
  barrier_init(&barrier, opt.thread_num + 1);
  for (i = 0; i < opt.thread_num; i++) {
      printf("Creating thread %d\n", i);
      create_thread(data + i, threads + i, opt, set, &barrier, operations, i);
  }
	
  /* Start threads */
  printf("crossing barrier in main thread\n");
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

  printf("Sorting operations array for replay\n");
  sort_by_timestamp(operations, opt.add_operations + opt.del_operations + opt.read_operations);
  
  for (i = 1; i < opt.add_operations + opt.del_operations + opt.read_operations; i++)
  {
    //assert(operations[i].completed_timestamp < operations[i-1].completed_timestamp);
  }
	
  print_stats(data, opt.thread_num, set);

  // replay and output the results
  data[0].set = set_new();
  replay(&data[0]);

  char ideal[strlen(opt.filename) + 100];
  char parallel[strlen(opt.filename) + 100];
  strcpy(ideal, opt.filename);
  strcpy(parallel, opt.filename);

  dump_operations(operations, data[0].num_ops, strcat(ideal, "_ideal.dat"), IS_IDEAL);
  dump_operations(operations, data[0].num_ops, strcat(parallel, "_parallel.dat"), IS_PARALLEL);
  /* Delete set */
  set_delete(set);
	
  free(threads);
  free(data);
	
  return 0;
}

void print_stats(thread_data_t* data, int nb_threads, intset_t *set) {

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
  printf("Set size      : %d (expected: %d)\n", set_size(set), size);
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
	intset_t *set, barrier_t *barrier, operation_t *operations, int thread_id) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    data->thread_id = thread_id;
    data->range = opt.range;
    data->unit_tx = DEFAULT_ELASTICITY;
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

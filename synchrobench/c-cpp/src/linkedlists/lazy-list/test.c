/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses to skip list integer set
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

#include "intset.h"
#include <stdatomic.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

unsigned long global_seed;
#ifdef TLS
__thread unsigned long *rng_seed;
#else /* ! TLS */
pthread_key_t rng_seed_key;
#endif /* ! TLS */

/* Entropy Declarations */

#define OP_CONTAINS   0
#define OP_ADD        1
#define OP_REMOVE     2
#define OP_UNDEFINED  3

typedef struct history_item {
	struct timeval  start;
	unsigned char      op;
	long              arg;
	unsigned char  result;
};

void printHistItem(char * buf
					, const unsigned char op
					, const unsigned char result) {

	char op_buf[20];
	switch (op) {
	case OP_CONTAINS:
		sprintf(op_buf, "%s", "CONTAINS");
	break;
	case OP_ADD:
		sprintf(op_buf, "%s", "ADD");
	break;
	case OP_REMOVE:
		sprintf(op_buf, "%s", "REMOVE");
	break;
	}

	sprintf(buf, "%s %d", op_buf, result);
}

#define DEFAULT_NUM_OPERATIONS      100000
#define DEFAULT_ADD_OPERATIONS      10000
#define DEFAULT_CONTAINS_OPERATIONS 80000
#define MAX_THREADS 1000

long num_operations = DEFAULT_NUM_OPERATIONS;
long num_add_operations = DEFAULT_ADD_OPERATIONS;
long num_contain_operations = DEFAULT_CONTAINS_OPERATIONS;

long history_len = 0;

long inversions[MAX_THREADS];
struct history_item * history;

const long num_of_runs = 1000;
long runs = 0;

/* End Entropy Declarations */

typedef struct barrier {
	pthread_cond_t complete;
	pthread_mutex_t mutex;
	long count;
	long crossing;
} barrier_t;

void barrier_init(barrier_t *b, long n)
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
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
inline long rand_range(long r) {
	long m = RAND_MAX - num_operations;
	long d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v + num_operations;
}
long rand_range(long r);

/* Thread-safe, re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned long *seed, long r) {
	long m = RAND_MAX - num_operations;
	long d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v + num_operations;
}
long rand_range_re(unsigned long *seed, long r);

typedef struct thread_data {
	long thread_num;
	val_t first;
	long range;
	long update;
	long unit_tx;
	long alternate;
	long effective;
	atomic_ulong nb_add;
	atomic_ulong nb_added;
	atomic_ulong nb_remove;
	atomic_ulong nb_removed;
	atomic_ulong nb_contains;
	atomic_ulong nb_found;
	unsigned long nb_aborts;
	unsigned long nb_aborts_locked_read;
	unsigned long nb_aborts_locked_write;
	unsigned long nb_aborts_validate_read;
	unsigned long nb_aborts_validate_write;
	unsigned long nb_aborts_validate_commit;
	unsigned long nb_aborts_invalid_memory;
	unsigned long nb_aborts_double_write;
	unsigned long max_retries;
	unsigned long seed;
	sigset_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;


#define MIN(a,b) ((a)<(b)?(a):(b))

FILE * pl_logFileSeq = NULL, * pl_logFileAct = NULL;

long play_history(sigset_t *set, long unit_tx, long from, long to) {

	long hist_item = from, surprises = 0, result = 0;
	char buf_item[64];

	while (hist_item < to) {

		switch (history[hist_item].op) {
			case OP_ADD:
				result = set_add_l(set, history[hist_item].arg, unit_tx);
			break;
			case OP_REMOVE:
				result = set_remove_l(set, history[hist_item].arg, unit_tx);
			break;
			case OP_CONTAINS:
				result = set_contains_l(set, history[hist_item].arg, unit_tx);
			break;
		}

		printHistItem(buf_item, history[hist_item].op, history[hist_item].result);
		fprintf(pl_logFileAct, "%s\n", buf_item);

		printHistItem(buf_item, history[hist_item].op, result);
		fprintf(pl_logFileSeq, "%s\n", buf_item);

		if (result != history[hist_item].result) {
			++surprises;
		}

		++hist_item;
	}

	++inversions[surprises];

	return surprises;
}

void *test(void *data) {

	thread_data_t *d = (thread_data_t *)data;
	
	/* Wait on barrier */
	barrier_cross(d->barrier);
	
	while (1) {

		/* Record history (start time, operation) */
		long hist_item = atomic_fetch_add(&history_len, 1);

		if (hist_item < num_operations) {

			long result = 0;

			/* Record history (start time, operation) */
			gettimeofday(&history[hist_item].start, NULL);

			switch (history[hist_item].op) {
			case 1:

				if (result = set_add_l(d->set, history[hist_item].arg, TRANSACTIONAL)) {
					d->nb_added++;
				}
				d->nb_add++;

			break;
			case 2:

				history[hist_item].op = OP_REMOVE;
				if (result = set_remove_l(d->set, history[hist_item].arg, TRANSACTIONAL)) {
					d->nb_removed++;
				}
				d->nb_remove++;

			break;
			case 0:

				history[hist_item].op = OP_CONTAINS;
				if (set_contains_l(d->set, history[hist_item].arg, TRANSACTIONAL))
					d->nb_found++;
				d->nb_contains++;

			break;
			}

			/* Updated recorded history item */
			if (hist_item < num_operations) {
				history[hist_item].result = result;
			}

		} else
		if (hist_item >= num_operations) {

#ifdef BARRIER_RUN
			if (d->thread_num > 0) {

				barrier_cross(d->barrier);
				barrier_cross(d->barrier);

				struct timespec timeout;
				timeout.tv_sec = 0;
				timeout.tv_nsec = 1000;
				nanosleep(&timeout, NULL);

				if (runs >= num_of_runs) {
					break;
				}
			} else {

				barrier_cross(d->barrier);

				long item = 0;
				for (;item<num_operations;++item) {
					set_remove_l(d->set, item, TRANSACTIONAL);
				}

				play_history(d->set, TRANSACTIONAL);
				atomic_fetch_add(&runs, 1);
				hist_item = 0;

				item = 0;
				while (item < DEFAULT_INITIAL) {
					long val = rand_range_re(&global_seed, DEFAULT_RANGE) + num_operations;
					set_add_l(d->set, val, 0);
					item++;
				}

				barrier_cross(d->barrier);

				if (runs >= num_of_runs) {
					break;
				}
			}
#else
			barrier_cross(d->barrier);
			break;
#endif

		}
	}
	
	return NULL;
}

void catcher(long sig)
{
	printf("CAUGHT SIGNAL %d\n", sig);
}

long main(long argc, char **argv)
{
	struct option long_options[] = {
		// These options don't set a flag
		{"help",                      no_argument,       NULL, 'h'},
		{"duration",                  required_argument, NULL, 'd'},
		{"initial-size",              required_argument, NULL, 'i'},
		{"num-threads",               required_argument, NULL, 'n'},
		{"range",                     required_argument, NULL, 'r'},
		{"seed",                      required_argument, NULL, 's'},
		{"update-rate",               required_argument, NULL, 'u'},
		{"elasticity",                required_argument, NULL, 'x'},

		{"total-operations",          required_argument, NULL, 'o'},
		{"add-operations",            required_argument, NULL, 'a'},
		{"contain-operations",        required_argument, NULL, 'c'},
		{NULL, 0, NULL, 0}
	};
	
	sigset_t *set, *mirror_set;
	long i, c;
        unsigned long size;
	val_t last = 0; 
	val_t val = 0;
	unsigned long reads, effreads, updates, effupds, aborts, aborts_locked_read, aborts_locked_write,
	aborts_validate_read, aborts_validate_write, aborts_validate_commit,
	aborts_invalid_memory, aborts_double_write, max_retries, failures_because_contention;
	thread_data_t *data;
	pthread_t *threads;
	pthread_attr_t attr;
	barrier_t barrier;
	struct timeval start, end;
	struct timespec timeout;
	long duration = DEFAULT_DURATION;
	long initial = DEFAULT_INITIAL;
	long nb_threads = DEFAULT_NB_THREADS;
	long range = DEFAULT_RANGE;
	long seed = DEFAULT_SEED;
	long update = DEFAULT_UPDATE;
	long unit_tx = DEFAULT_LOCKTYPE;
	long alternate = DEFAULT_ALTERNATE;
	long effective = DEFAULT_EFFECTIVE;
	sigset_t block_set;
	
	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hAf:d:i:t:r:S:u:x:o:a:c:"
										, long_options, &i);
		
		if(c == -1)
			break;
		
		if(c == 0 && long_options[i].flag == 0)
			c = long_options[i].val;
		
		switch(c) {
				case 0:
					break;
				case 'h':
					printf("intset -- STM stress test "
								 "(skip list)\n"
								 "\n"
								 "Usage:\n"
								 "  intset [options...]\n"
								 "\n"
								 "Options:\n"
								 "  -h, --help\n"
								 "        Prlong this message\n"
								 "  -A, --Alternate\n"
								 "        Consecutive insert/remove target the same value\n"
								 "  -f, --effective <int>\n"
								 "        update txs must effectively write (0=trial, 1=effective, default=" XSTR(DEFAULT_EFFECTIVE) ")\n"
								 "  -d, --duration <int>\n"
								 "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
								 "  -i, --initial-size <int>\n"
								 "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
								 "  -t, --thread-num <int>\n"
								 "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
								 "  -r, --range <int>\n"
								 "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
								 "  -S, --seed <int>\n"
								 "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
								 "  -u, --update-rate <int>\n"
								 "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
								 "  -x, --elasticity (default=4)\n"
								 "        Use elastic transactions\n"
								 "        0 = non-protected,\n"
								 "        1 = normal transaction,\n"
								 "        2 = read elastic-tx,\n"
								 "        3 = read/add elastic-tx,\n"
								 "        4 = read/add/rem elastic-tx,\n"
								 "        5 = fraser lock-free\n"
								 "  -o, --total-operations\n"
								 "  -a, --add-operations\n"
								 "  -c, --contain-operations\n"
								 );
					exit(0);
				case 'A':
					alternate = 1;
					break;
				case 'f':
					effective = atoi(optarg);
					break;
				case 'd':
					duration = atoi(optarg);
					break;
				case 'i':
					initial = atoi(optarg);
					break;
				case 't':
					nb_threads = atoi(optarg);
					break;
				case 'r':
					range = atol(optarg);
					break;
				case 'S':
					seed = atoi(optarg);
					break;
				case 'u':
					update = atoi(optarg);
					break;
				case 'x':
					unit_tx = atoi(optarg);
					break;
				case '?':
					printf("Use -h or --help for help\n");
					exit(0);
				default:
					exit(1);
		}
	}
	
	assert(duration >= 0);
	assert(initial >= 0);
	assert(nb_threads > 0);
	assert(range > 0 && range >= initial);
	assert(update >= 0 && update <= 100);
	
	printf("Set type     : skip list\n");
	printf("Duration     : %d\n", duration);
	printf("Initial size : %u\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Value range  : %ld\n", range);
	printf("Seed         : %d\n", seed);
	printf("Update rate  : %d\n", update);
	printf("Elasticity   : %d\n", unit_tx);
	printf("Alternate    : %d\n", alternate);
	printf("Efffective   : %d\n", effective);
	printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
				 (int)sizeof(int),
				 (int)sizeof(long),
				 (int)sizeof(void *),
				 (int)sizeof(uintptr_t));
	
	timeout.tv_sec = duration / 1000;
	timeout.tv_nsec = (duration % 1000) * 1000000;
	
	history = (struct history_item *)malloc(num_operations * sizeof(struct history_item));
	if (NULL == history) {
		perror("malloc");
		exit(1);
	}

	if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
		perror("malloc");
		exit(1);
	}
	if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
		perror("malloc");
		exit(1);
	}
	
	pl_logFileSeq = fopen("seqOperations.log", "w+");
	pl_logFileAct = fopen("actOperations.log", "w+");

	if (seed == 0)
		srand((int)time(0));
	else
		srand(seed);
	
	set = set_new_l();
	mirror_set = set_new_l();
	stop = 0;
	
	memset(&inversions, sizeof(inversions), 0);

	global_seed = rand();
#ifdef TLS
	rng_seed = &global_seed;
#else /* ! TLS */
	if (pthread_key_create(&rng_seed_key, NULL) != 0) {
		fprintf(stderr, "Error creating thread local\n");
		exit(1);
	}
	pthread_setspecific(rng_seed_key, &global_seed);
#endif /* ! TLS */
	
	// Init STM 
	printf("Initializing STM\n");
	
	// Populate set 
	printf("Adding %d entries to set\n", initial);
	i = 0;
	
	while (i < initial) {
		val = rand_range_re(&global_seed, range);
		if (set_add_l(set, val, 0)) {
			last = val;
			i++;
		}
		set_add_l(mirror_set, val, 0);
	}
	size = set_size_l(set);
	printf("Set size     : %lu\n", size);
	
	// Access set from all threads 
	barrier_init(&barrier, nb_threads + 1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		printf("Creating thread %d\n", i);
		data[i].thread_num = i;
		data[i].first = last;
		data[i].range = range;
		data[i].update = update;
		data[i].unit_tx = unit_tx;
		data[i].alternate = alternate;
		data[i].effective = effective;
		data[i].nb_add = 0;
		data[i].nb_added = 0;
		data[i].nb_remove = 0;
		data[i].nb_removed = 0;
		data[i].nb_contains = 0;
		data[i].nb_found = 0;
		data[i].nb_aborts = 0;
		data[i].nb_aborts_locked_read = 0;
		data[i].nb_aborts_locked_write = 0;
		data[i].nb_aborts_validate_read = 0;
		data[i].nb_aborts_validate_write = 0;
		data[i].nb_aborts_validate_commit = 0;
		data[i].nb_aborts_invalid_memory = 0;
		data[i].nb_aborts_double_write = 0;
		data[i].max_retries = 0;
		data[i].seed = rand();
		data[i].set = set;
		data[i].barrier = &barrier;
		data[i].failures_because_contention = 0;
		if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
			fprintf(stderr, "Error creating thread\n");
			exit(1);
		}
	}
	pthread_attr_destroy(&attr);
	
	// Catch some signals 
	if (signal(SIGHUP, catcher) == SIG_ERR ||
			//signal(SIGINT, catcher) == SIG_ERR ||
			signal(SIGTERM, catcher) == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	srand(time(0));
	long hist_item = 0;
	for (;hist_item<num_operations;++hist_item) {
		history[hist_item].op = OP_REMOVE;
		history[hist_item].arg = (rand() % nb_threads) + (hist_item / nb_threads) * nb_threads;
	}

	long add_item = 0;
	for (;add_item<num_add_operations;++add_item) {
		history[rand() % num_operations].op = OP_ADD;
	}

	long contains_item = 0;
	for (;contains_item<num_contain_operations;++contains_item) {
		long cur_index = (rand() % num_contain_operations);
		while (history[cur_index].op != OP_REMOVE
			   && cur_index < num_operations) {
			++cur_index;
		}
		if (cur_index<num_operations) {
			history[cur_index].op = OP_CONTAINS;
		}
	}

	// Start threads 
	barrier_cross(&barrier);
	
	printf("STARTING...\n");
	gettimeofday(&start, NULL);

#ifdef BARRIER_RUN
	long nruns = 0;
	while (nruns++ < num_of_runs) {
		barrier_cross(&barrier);
		barrier_cross(&barrier);
	}
#else
	barrier_cross(&barrier);
	long n_hist = 0;
	for (; n_hist < num_operations/nb_threads; ++n_hist) {

		long from = n_hist * nb_threads, to = (n_hist + 1) * nb_threads;
		long surprises = play_history(mirror_set, unit_tx, from, to);
		++inversions[surprises];
	}
#endif

	gettimeofday(&end, NULL);
	printf("STOPPING...\n");
	
	// Wait for thread completion 
	for (i = 0; i < nb_threads; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "Error waiting for thread completion\n");
			exit(1);
		}
	}
	
	printf("PLAYING HISTORY...\n");

	/* Play history and count "surprises" */
	duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
	aborts = 0;
	aborts_locked_read = 0;
	aborts_locked_write = 0;
	aborts_validate_read = 0;
	aborts_validate_write = 0;
	aborts_validate_commit = 0;
	aborts_invalid_memory = 0;
	aborts_double_write = 0;
	failures_because_contention = 0;
	reads = 0;
	effreads = 0;
	updates = 0;
	effupds = 0;
	max_retries = 0;
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
		printf("    #dup-w    : %lu\n", data[i].nb_aborts_double_write);
		printf("    #failures : %lu\n", data[i].failures_because_contention);
		printf("  Max retries : %lu\n", data[i].max_retries);
		aborts += data[i].nb_aborts;
		aborts_locked_read += data[i].nb_aborts_locked_read;
		aborts_locked_write += data[i].nb_aborts_locked_write;
		aborts_validate_read += data[i].nb_aborts_validate_read;
		aborts_validate_write += data[i].nb_aborts_validate_write;
		aborts_validate_commit += data[i].nb_aborts_validate_commit;
		aborts_invalid_memory += data[i].nb_aborts_invalid_memory;
		aborts_double_write += data[i].nb_aborts_double_write;
		failures_because_contention += data[i].failures_because_contention;
		reads += data[i].nb_contains;
		effreads += data[i].nb_contains + 
		(data[i].nb_add - data[i].nb_added) + 
		(data[i].nb_remove - data[i].nb_removed); 
		updates += (data[i].nb_add + data[i].nb_remove);
		effupds += data[i].nb_removed + data[i].nb_added; 
		size += data[i].nb_added - data[i].nb_removed;
		if (max_retries < data[i].max_retries)
			max_retries = data[i].max_retries;
	}
	printf("Set size      : %lu (expected: %lu)\n", set_size_l(set), size);
	printf("Duration      : %d (ms)\n", duration);
	printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
	
	printf("#read txs     : ");
	if (effective) {
		printf("%lu (%f / s)\n", effreads, effreads * 1000.0 / duration);
		printf("  #contains   : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
	} else printf("%lu (%f / s)\n", reads, reads * 1000.0 / duration);
	
	printf("#eff. upd rate: %f \n", 100.0 * effupds / (effupds + effreads));
	
	printf("#update txs   : ");
	if (effective) {
		printf("%lu (%f / s)\n", effupds, effupds * 1000.0 / duration);
		printf("  #upd trials : %lu (%f / s)\n", updates, updates * 1000.0 / 
					 duration);
	} else printf("%lu (%f / s)\n", updates, updates * 1000.0 / duration);
	
	printf("#aborts       : %lu (%f / s)\n", aborts, aborts * 1000.0 / duration);
	printf("  #lock-r     : %lu (%f / s)\n", aborts_locked_read, aborts_locked_read * 1000.0 / duration);
	printf("  #lock-w     : %lu (%f / s)\n", aborts_locked_write, aborts_locked_write * 1000.0 / duration);
	printf("  #val-r      : %lu (%f / s)\n", aborts_validate_read, aborts_validate_read * 1000.0 / duration);
	printf("  #val-w      : %lu (%f / s)\n", aborts_validate_write, aborts_validate_write * 1000.0 / duration);
	printf("  #val-c      : %lu (%f / s)\n", aborts_validate_commit, aborts_validate_commit * 1000.0 / duration);
	printf("  #inv-mem    : %lu (%f / s)\n", aborts_invalid_memory, aborts_invalid_memory * 1000.0 / duration);
	printf("  #dup-w      : %lu (%f / s)\n", aborts_double_write, aborts_double_write * 1000.0 / duration);
	printf("  #failures   : %lu\n",  failures_because_contention);
	printf("Max retries   : %lu\n", max_retries);
	printf("Entropy:\n");
	
	double entropy = 0;
	long spr = 1;

	for (; spr < nb_threads+1; ++spr) {
		printf("  # %d-surprises : %d \n", spr, inversions[spr]);
		if (inversions[spr]) {
			double Px = (double)inversions[spr] / (double)spr;
			entropy -= Px * (log10(Px));
		}
	}

	printf("  #ENTROPY: %f \n", entropy);

	// Delete set 
    set_delete_l(set);
    set_delete_l(mirror_set);
	
#ifndef TLS
	pthread_key_delete(rng_seed_key);
#endif /* ! TLS */
	
	fclose(pl_logFileSeq);
	fclose(pl_logFileAct);

	free(threads);
	free(data);
	free(history);
	
	return 0;
}


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

volatile AO_t stop;
unsigned int global_seed;
#ifdef TLS
__thread unsigned int *rng_seed;
#else /* ! TLS */
pthread_key_t rng_seed_key;
#endif /* ! TLS */
unsigned int levelmax;

#define OP_CONTAINS   0
#define OP_ADD        1
#define OP_REMOVE     2

typedef struct history_item {
	struct timeval start;
	unsigned char     op;
	int              arg;
	int           result;
};

#define MAX_HISTORY_ITEM 1000

int history_len = 0;

int inversions[MAX_HISTORY_ITEM];
struct history_item * history;

const int num_of_runs = 1000;
int runs = 0;

typedef struct barrier {
	pthread_cond_t complete;
	pthread_mutex_t mutex;
	int count;
	int crossing;
} barrier_t;

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
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
inline long rand_range(long r) {
	int m = RAND_MAX - MAX_HISTORY_ITEM;
	int d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v + MAX_HISTORY_ITEM;
}
long rand_range(long r);

/* Thread-safe, re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
	int m = RAND_MAX - MAX_HISTORY_ITEM;
	int d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v + MAX_HISTORY_ITEM;
}
long rand_range_re(unsigned int *seed, long r);

typedef struct thread_data {
	int thread_num;
	val_t first;
	long range;
	int update;
	int unit_tx;
	int alternate;
	int effective;
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
	unsigned long nb_aborts_double_write;
	unsigned long max_retries;
	unsigned int seed;
	sl_intset_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;


void print_skiplist(sl_intset_t *set) {
	sl_node_t *curr;
	int i, j;
	int arr[levelmax];
	
	for (i=0; i< sizeof arr/sizeof arr[0]; i++) arr[i] = 0;
	
	curr = set->head;
	do {
		printf("%d", (int) curr->val);
		for (i=0; i<curr->toplevel; i++) {
			printf("-*");
		}
		arr[curr->toplevel-1]++;
		printf("\n");
		curr = curr->next[0];
	} while (curr); 
	for (j=0; j<levelmax; j++)
		printf("%d nodes of level %d\n", arr[j], j);
}

#define MIN(a,b) ((a)<(b)?(a):(b))

int play_history(sl_intset_t *set, int unit_tx, int from, int to) {

	int hist_item = from, surprises = 0, result = 0;

	while (hist_item < to) {

		switch (history[hist_item].op) {
			case OP_ADD:
				result = sl_add(set, history[hist_item].arg, unit_tx);
			break;
			case OP_REMOVE:
				result = sl_remove(set, history[hist_item].arg, unit_tx);
			break;
			case OP_CONTAINS:
				result = sl_contains(set, history[hist_item].arg, unit_tx);
			break;
		}

		if (result != history[hist_item].result) {
			++surprises;
		}

//		printf("itme %d op %d arg %d result %d\n", hist_item, history[hist_item].op, history[hist_item].arg, history[hist_item].result);

		++hist_item;
	}

	++inversions[surprises];

	return surprises;
}

void *test(void *data) {
	thread_data_t *d = (thread_data_t *)data;
	
	/* Create transaction */
	TM_THREAD_ENTER();
	/* Wait on barrier */
	barrier_cross(d->barrier);
	
	while (1) {

		/* Record history (start time, operation) */
		int hist_item = atomic_fetch_add(&history_len, 1);

		if (hist_item < MAX_HISTORY_ITEM) {

			int result = 0;

			/* Record history (start time, operation) */
			gettimeofday(&history[hist_item].start, NULL);

			switch (history[hist_item].op) {
			case 1:

				if (result = sl_add(d->set, history[hist_item].arg, TRANSACTIONAL)) {
					d->nb_added++;
				}
				d->nb_add++;

			break;
			case 2:

				history[hist_item].op = OP_REMOVE;
				if (result = sl_remove(d->set, history[hist_item].arg, TRANSACTIONAL)) {
					d->nb_removed++;
				}

			break;
			case 0:

				history[hist_item].op = OP_CONTAINS;
				if (sl_contains(d->set, history[hist_item].arg, TRANSACTIONAL))
					d->nb_found++;
				d->nb_contains++;

			break;
			}

			/* Updated recorded history item */
			if (hist_item < MAX_HISTORY_ITEM) {
				history[hist_item].result = result;
			}

		} else
		if (hist_item >= MAX_HISTORY_ITEM) {

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

				int item = 0;
				for (;item<MAX_HISTORY_ITEM;++item) {
					sl_remove(d->set, item, TRANSACTIONAL);
				}

				play_history(d->set, TRANSACTIONAL);
				atomic_fetch_add(&runs, 1);
				hist_item = 0;

				item = 0;
				while (item < DEFAULT_INITIAL) {
					int val = rand_range_re(&global_seed, DEFAULT_RANGE) + MAX_HISTORY_ITEM;
					sl_add(d->set, val, 0);
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
	
	/* Free transaction */
	TM_THREAD_EXIT();
	
	return NULL;
}

void catcher(int sig)
{
	printf("CAUGHT SIGNAL %d\n", sig);
}

int main(int argc, char **argv)
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
		{NULL, 0, NULL, 0}
	};
	
	sl_intset_t *set, *mirror_set;
	int i, c;
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
	int duration = DEFAULT_DURATION;
	int initial = DEFAULT_INITIAL;
	int nb_threads = DEFAULT_NB_THREADS;
	long range = DEFAULT_RANGE;
	int seed = DEFAULT_SEED;
	int update = DEFAULT_UPDATE;
	int unit_tx = DEFAULT_ELASTICITY;
	int alternate = DEFAULT_ALTERNATE;
	int effective = DEFAULT_EFFECTIVE;
	sigset_t block_set;
	
	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hAf:d:i:t:r:S:u:x:"
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
								 "        Print this message\n"
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
	
	history = (struct history_item *)malloc(MAX_HISTORY_ITEM * sizeof(struct history_item));
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
	
	if (seed == 0)
		srand((int)time(0));
	else
		srand(seed);
	
	levelmax = floor_log_2((unsigned int) initial);
	set = sl_set_new();
	mirror_set = sl_set_new();
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
	
	TM_STARTUP();
	
	// Populate set 
	printf("Adding %d entries to set\n", initial);
	i = 0;
	
	while (i < initial) {
		val = rand_range_re(&global_seed, range);
		if (sl_add(set, val, 0)) {
			last = val;
			i++;
		}
		sl_add(mirror_set, val, 0);
	}
	size = sl_set_size(set);
	printf("Set size     : %lu\n", size);
	printf("Level max    : %d\n", levelmax);
	
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
	int hist_item = 0;
	for (;hist_item<MAX_HISTORY_ITEM;++hist_item) {

		history[hist_item].op  = rand() % 3;
		history[hist_item].arg = (rand() % nb_threads) + (hist_item / nb_threads) * nb_threads;
	}

	// Start threads 
	barrier_cross(&barrier);
	
	printf("STARTING...\n");
	gettimeofday(&start, NULL);

#ifdef BARRIER_RUN
	int nruns = 0;
	while (nruns++ < num_of_runs) {
		barrier_cross(&barrier);
		barrier_cross(&barrier);
	}
#else
	barrier_cross(&barrier);
	int n_hist = 0;
	for (; n_hist < MAX_HISTORY_ITEM/nb_threads; ++n_hist) {

		int from = n_hist * nb_threads, to = (n_hist + 1) * nb_threads;
		int surprises = play_history(mirror_set, unit_tx, from, to);
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
	printf("Set size      : %lu (expected: %lu)\n", sl_set_size(set), size);
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
	int spr = 1;

	for (; spr < nb_threads+1; ++spr) {
		printf("  # %d-surprises : %d \n", spr, inversions[spr]);
		if (inversions[spr]) {
			double Px = (double)inversions[spr] / (double)spr;
			entropy -= Px * (log10(Px));
		}
	}

	printf("  #ENTROPY: %f \n", entropy);

	// Delete set 
    sl_set_delete(set);
    sl_set_delete(mirror_set);
	
	// Cleanup STM 
	TM_SHUTDOWN();
	
#ifndef TLS
	pthread_key_delete(rng_seed_key);
#endif /* ! TLS */
	
	free(threads);
	free(data);
	free(history);
	
	return 0;
}


#include "testutils.h"
#include "test.h"


long long current_time_ms (void)
{
    long            ms; // Milliseconds
    time_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s  = spec.tv_sec;
    ms = spec.tv_nsec / 1.0e6; // Convert nanoseconds to milliseconds
    if (ms > 999) {
        s++;
        ms = 0;
    }

    return (long long)s * 1000 + ms;
}

int opid = 0;

void reset_tests() {
    opid = 0;
}

void *test(void *data) {
    thread_data_t *d = (thread_data_t *)data;

    /* Wait on barrier */
    barrier_cross(d->barrier);

    while (1) {
	int this_op = atomic_fetch_add(&opid, 1);
	if (this_op > d->num_ops) {
		break;
	}

	operation_t *op = &d->ops[this_op-1];

	switch (op->type) {
	    case ADD:
		op->result = set_add_l(d->set, op->arg, TRANSACTIONAL);
		if (op->result) d->nb_added++;
		d->nb_add++;
		break;
	    case DELETE:
		op->result = set_remove_l(d->set, op->arg, TRANSACTIONAL);
		if (op->result) d->nb_removed++;
		d->nb_remove++;
		break;
	    case READ:
		op->result = set_contains_l(d->set, op->arg, TRANSACTIONAL);
		if (op->result) d->nb_found++;
		d->nb_contains++;
		break;
	    default:
		assert(0);
	}
	op->completed_timestamp = current_time_ms();
        
    } 
			
    return NULL;
}

operation_t *prepare_test(options_t opt, unsigned int *seed) {
    int num_ops = opt.add_operations + opt.del_operations + opt.read_operations;
    operation_t *operations = (operation_t*)malloc(num_ops * sizeof (operation_t));
    assert(operations);
    
    int i;
    int index = 0;
    for (i = 0; i < opt.add_operations; i++) {
        operations[index].type = ADD;
	operations[index].arg = rand_range_re(seed, opt.range);
	index++;
    }
    for (i = 0; i < opt.del_operations; i++) {
        operations[index].type = DELETE;
	operations[index].arg = rand_range_re(seed, opt.range);
	index++;
    }
    for (i = 0; i < opt.read_operations; i++) {
        operations[index].type = READ;
	operations[index].arg = rand_range_re(seed, opt.range);
	index++;
    }

    // scramble.
    for (i = num_ops-1; i >= 0; i--) {
	int si = rand_range_re(seed, i+1) - 1;
	operation_t tmp = operations[si];
	operations[si] = operations[i];
	operations[i] = tmp;
    }

    return operations;
}

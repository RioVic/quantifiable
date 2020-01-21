#include "testutils.h"
#include "test.h"
#include "stdio.h"

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
		op->replay_result = set_add_l(d->set, op->arg, TRANSACTIONAL);
		break;
	    case DELETE:
		op->replay_result = set_remove_l(d->set, op->arg, TRANSACTIONAL);
		break;
	    case READ:
		op->replay_result = set_contains_l(d->set, op->arg, TRANSACTIONAL);
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

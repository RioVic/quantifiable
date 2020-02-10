#ifndef OPTIONS_H
#define OPTIONS_H

#include "testutils.h"

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

options_t read_options(int argc, char** argv);
#endif

#ifndef OPTIONS_H
#define OPTIONS_H
#include "testutils.h"

#define DEFAULT_ADD_OPERATIONS 10000
#define DEFAULT_DEL_OPERATIONS 10000
//#define DEFAULT_RANGE 256
#define DEFAULT_THREAD_NUM 8
#define DEFAULT_SEED 0 

typedef struct options {
	int add_operations;
	int del_operations;
	long range;
	int seed;
	int thread_num;
} options_t;

options_t read_options(int argc, char** argv);
#endif

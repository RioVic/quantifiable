# Overview

Synchrobench contains many examples of concurrent data structures, with an integer set implementation of each one.

The test framework consists of 2 major steps:

1. INTRUMENTATION of synchrobench structures by logging timestamps and function call results

2. ANALYSIS the results by counting inversions and entropy, and graphing.

# INSTRUMENTATION

## Building

The code for instrumentation lives under

	synchrobench/c-cpp/src/entropy/

type

	$ make

to build.

It is currently configured for the structure at

	synchrobench/c-cpp/src/linkedlists/lazy-list/

## Building instrumentation for another structure

To configure for another data structure, 

1. Modify the Makefile of the data structure to produce a static library. For example, in lazy list, this would mean adding the line:

	ar rcs $(BUILDIR)/$(LOCK)-lazy-list.a $(BUILDIR)/linkedlist-lock.o $(BUILDIR)/lazy.o $(BUILDIR)/coupling.o $(BUILDIR)/intset.o

2. When compiling the instrumentation, use the SUBJECT_DIR and ARTIFACT env variables to configure what to compile and output. For example:

	$ ARTIFACT=lazy-list SUBJECT_DIR=linkedlist/lazy-list make

The above step would produce a binary under synchrobench/c-cpp/bin/MUTEX-lazy-list

## Running the instrumentation

Run the executable. It will create 2 files: 

ops_parallel.dat - the raw experiment results
ops_ideal.dat - the single threaded result.

There are several parameters that can be changed:

-t thread_num
-R range of numbers used (from 1 to range)
-S random seed (default to time)
-a # add operations
-d # delete operations
-r # read operations
-o output filename prefix (ops by default)

Planning to add flags to mimic consumer / producer behavior.

# 2. ANALYSIS

The code for counting and sizing inversions is under

	synchrobench/Scripts/
	
entropy.cpp contains the code that counts the inversions given the parallel.dat and ideal.dat files.

build it using

	$ make

and run it with

	$ ./a.out ops_parallel.dat ops_ideal.dat 1
	
which produces 

	ops_inversions.dat

## Graphing

Use the R script at

	synchrobench/Scripts/run_experiment/graph.R 
	
to create graphs using the ops_inversions.dat. For example:

	$ Rscript graph.R result.pdf ops_inversions.dat
	
You can include multiple inversion data files with the properties in the filename separated by underscore _, and the program will produce a comparison graph with the discriminating fields in the legend. For example:

	$ Rscript graph.R result.pdf ops_4thread_inversions.dat ops_8thread_inversions.dat

# Bulk experimentation scripts

I've been working on running bulk experiments at

	synchrobench/Scripts/run_experiment/
	
There are two scripts. The first one creates results under run_experiment/results:

	./run_benchmarks.sh

Runs a series of benchmarks over the variables "number of threads", "# distinguised items" and "# operations". #operations is divided into 1/4 add, 1/4 remove, and 1/2 read.
The script also generates the inversion data files.

You can configure what synchrobench data structure is used by setting the ARTIFACT env variable. But the instrumentation must have been built beforehand and placed under synchrobench/c-cpp/bin/ (see Instrumentation -> Building).

The second script produces graphs of all the results under run_expriment/results:

	./generate_graphs.sh
	
You can go into the subfolders of results/ to view the resulting graphs.


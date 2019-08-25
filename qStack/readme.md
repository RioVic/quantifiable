## Dependencies:
Boost library
pthreads

## To build:

$make

## For help running:

$./a.out --help

size of operation intervals - The size of each pairwise chunk of enqueues/deques - k.
number of intervals - The number of times the pariwise chunk of operations will be repeated - n. 

Running ./a.out will generate two files. *_ideal.dat and *_parallel.dat. In order to count inversions, these files must both be moved to the ../Scripts directory before running the analysis program located there.

## Discussion of experiment design:

Memory is preallocated to isolate the entropy issues from global performance concerns.

When running a 100k-100k case on say, 8 threads, each of the 8 threads perform 100k/8 pushes followed by 100k/8 pops? It is the intuitive way to do it (Assuming threads work together well) but the problem is that when the threads get out of sync the test can slowly start to look more like a 1-1 pairwise test as time goes on.

We do not use a global barrier to indicate to all threads that the 100k pushes are done and to start popping.  This would  have the potential for creating cache misses and woudl slowdown the entiure experimetn, eventually becomeing several separate experiments.  In our runs we we have 100,000 preallocated the threads pick them up like 1,17,33,49 and so on

After completing the preallocated pushes, the first thread will start popping, resulting in a period of sequential pops until the second thread catches up, then many threads will pop at once in a steady state, then the leaders will start pushing again.

So for example, with 100k operations, there should only really be some mixing occurring near the overlap.  The middle 80k or so should probably be purely pushes or pops.   As the interval gets smaller it will gradually deteriorate into random push / pop because the threads will nto have time to get in sync doing the same operation.  Threads are likely to fall entirely out of sync as the subintervals become much shorter than the tests.

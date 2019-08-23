Dependencies:
Boost library
pthreads

To build:
$make

For help running:
$./a.out --help

size of operation intervals - The size of each pairwise chunk of enqueues/deques - k.
number of intervals - The number of times the pariwise chunk of operations will be repeated - n. 

Running ./a.out will generate two files. *_ideal.dat and *_parallel.dat. In order to count inversions, these files must both be moved to the ../Scripts directory before running the analysis program located there.


#To Build:#

	$ make

#To Run:#

	./<executable> <number_of_threads> <pairwise_interval> <pairwise_sets> <sequential_test>

#Entropy Testing:#

	The test executes n sets of x enqueues followed by x dequeues.
	n = pairwise_sets
	x = pairwise_interval

	First run the executable with sequential_test set to 0. This will generate a parallel history for the given parameters. Then, re-execute the SAME executable with the same parameters, but with sequential_test set to 1. The program will automatically find the correct parallel history file and re-run the test sequentially. 
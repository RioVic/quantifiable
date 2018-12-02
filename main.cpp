#include "qstack.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <boost/random.hpp>

QStack<int> *s;

void work(int thread_id, int num_ops, int push_ratio)
{
	boost::mt19937 randomGen;
    randomGen.seed(time(0));
    boost::uniform_int<uint32_t> randomDist(1,1000);

	for (int i = 0; i < num_ops; i++)
	{
		int r = randomDist(randomGen);
		bool result;

		if ((r%101) <= push_ratio)
		{
			result = s->push(thread_id, i, r);
		}
		else
		{
			int val;
			result = s->pop(thread_id, i, val);
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		std::cout << "Please use: " << argv[0] << " <number of threads> <number of operations> <percentage of pushes>\n";
		return -1;
	}

	int NUM_THREADS = atoi(argv[1]);
	int NUM_OPS = atoi(argv[2]);
	int RATIO_PUSH = atoi(argv[3]);

	s = new QStack<int>(NUM_THREADS, NUM_OPS);
	std::vector<std::thread> threads;

	for (int i = 0; i < NUM_THREADS; i++)
	{
		threads.push_back(std::thread(work, i, NUM_OPS/NUM_THREADS, RATIO_PUSH));
	}

	for (std::thread &t : threads)
		t.join();
}
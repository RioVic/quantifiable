#include "qstack.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <boost/random.hpp>
#include <iostream>
#include <fstream>
#include <time.h>

QStack<int> *s;

void work(int thread_id, int num_ops, int push_ratio)
{
	boost::mt19937 randomGen;
	randomGen.seed(time(0));
	boost::uniform_int<uint32_t> randomDist(1, 1000);

	for (int i = 0; i < num_ops; i++)
	{
		int r = randomDist(randomGen);
		bool result;

		if ((r % 100) < push_ratio)
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

	std::ofstream file;
	file.open("qstack.out");
	file << "type\tmix\tthreads\tms\tops\t\n";

	for (int k = 1; k <= NUM_THREADS; k++)
	{
		std::cout << k << "\n";
		for (int i = 0; i < 10; i++)
		{
			std::cout << "iter: " << i << "\n";
			std::vector<std::thread> threads;
			s = new QStack<int>(k, NUM_OPS);
			auto start = std::chrono::high_resolution_clock::now();

			for (int i = 0; i < k; i++)
			{
				threads.push_back(std::thread(work, i, NUM_OPS, RATIO_PUSH));
			}

			for (std::thread &t : threads)
				t.join();

			auto end = std::chrono::high_resolution_clock::now();
			auto elapsed = end-start;

			file << "qstack\t" << "0-100-0\t" << k << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS*k << "\n";
			delete s;
		}
	}
	

	/*
	for (int i : s->headIndexStats)
	{
		std::cout << i << "\n";
	}*/

	//file.open("dump.dat");
	//s->dumpNodes(file);
	//file.close();
}
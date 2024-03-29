#include "qqueue.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <boost/random.hpp>
#include <iostream>
#include <fstream>
#include <time.h>
#include <string>

QQueue<int> *q = nullptr;

template<class T>
void work(int thread_id, int num_ops, int push_ratio, T *s)
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
			result = s->enqueue(thread_id, i, r);
		}
		else
		{
			int val;
			result = s->dequeue(thread_id, i, val);
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		std::cout << "Please use: " << argv[0] << " <number of threads> <number of operations> <percentage of enqueues> <\"QQueue\"> \n";
		return -1;
	}

	int NUM_THREADS = atoi(argv[1]);
	int NUM_OPS = atoi(argv[2]);
	int RATIO_PUSH = atoi(argv[3]);
	char* MODE = argv[4];

	std::ofstream file;
	file.open(std::string(MODE) + std::to_string(RATIO_PUSH) + std::string(".dat"));
	file << "type\tmix\tthreads\tms\tops\t\n";

	for (int k = 1; k <= NUM_THREADS; k++)
	{
		std::cout << k << "\n";
		for (int i = 0; i < 10; i++)
		{
			std::cout << "iter: " << i << "\n";
			std::vector<std::thread> threads;

			if (strcmp(MODE, "QQueue") == 0)
			{
				QQueue<int> *s = new QQueue<int>(k, NUM_OPS);
				auto start = std::chrono::high_resolution_clock::now();

				for (int j = 0; j < k; j++)
					threads.push_back(std::thread(&work<QQueue<int>>, j, NUM_OPS, RATIO_PUSH, s));

				for (std::thread &t : threads)
					t.join();

				auto end = std::chrono::high_resolution_clock::now();
				auto elapsed = end-start;

				file << MODE << "\t" << RATIO_PUSH << "-" << (100-RATIO_PUSH) << "\t" << k << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS*k << "\n";
				delete s;
			}
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
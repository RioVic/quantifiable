#include "qstack.h"
#include "ebs.h"
#include "treiber_stack.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <boost/random.hpp>
#include <iostream>
#include <fstream>
#include <time.h>
#include <string>

QStack<int> *q = nullptr;
Treiber_S<int> *treiber = nullptr;
EliminationBackoffStack<int> *ebs = nullptr;

unsigned long *invocations;
unsigned long *returns;

inline unsigned long rdtsc() {
  volatile unsigned long tl;
  asm __volatile__("lfence\nrdtsc" : "=a" (tl): : "%edx"); //lfence is used to wait for prior instruction (optional)
  return tl;
}

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
		int val = -11;
		int popOpn;

		//Log invocation and return, as well as execute a random method
		unsigned long invoked = rdtsc();
		unsigned long returned;

		if ((r % 100) < push_ratio)
		{
			result = s->push(thread_id, i, r, val, popOpn);

			returned = rdtsc();
			if (val != -11)
			{
				//Find the pending pop, update its return timestamp
				//std::cout << "Pending pop found\n";
			}
		}
		else
		{
			result = s->pop(thread_id, i, val);

			if (val != -11)
				returned = rdtsc();
		}

		invocations[i] = invoked;
		returns[i] = returned;
	}
}

//Exports the history of the execution to file based on invocations and returns
void exportHistory()
{

}

int main(int argc, char** argv)
{
	if (argc < 5 || strcmp(argv[1],"--help") == 0)
	{
		std::cout << "Please use: " << argv[0] << " <number of threads> <number of operations> <percentage of pushes> <\"QStack\" | \"Treiber\" | \"EBS\"> \n";
		return -1;
	}

	int NUM_THREADS = atoi(argv[1]);
	int NUM_OPS = atoi(argv[2]);
	int RATIO_PUSH = atoi(argv[3]);
	char* MODE = argv[4];

	invocations = new unsigned long [NUM_OPS];
	returns = new unsigned long [NUM_OPS];

	std::ofstream file;
	file.open(std::string(MODE) + std::to_string(RATIO_PUSH) + std::string(".dat"), std::ios_base::app);

	std::vector<std::thread> threads;
	if (strcmp(MODE, "QStack") == 0)
	{
		QStack<int> *s = new QStack<int>(NUM_THREADS, NUM_OPS/NUM_THREADS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<QStack<int>>, j, NUM_OPS/NUM_THREADS, RATIO_PUSH, s));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;
		//s->dumpNodes(file);

		file << MODE << "\t" << RATIO_PUSH << "-" << (100-RATIO_PUSH) << "\t" << NUM_THREADS << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS << "\n";
		delete s;
	}
	else if (strcmp(MODE, "Treiber") == 0)
	{
		Treiber_S<int> *s = new Treiber_S<int>(NUM_THREADS, NUM_OPS/NUM_THREADS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<Treiber_S<int>>, j, NUM_OPS/NUM_THREADS, RATIO_PUSH, s));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		file << MODE << "\t" << RATIO_PUSH << "-" << (100-RATIO_PUSH) << "\t" << NUM_THREADS << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS << "\n";
		delete s;
	}
	else if (strcmp(MODE, "EBS") == 0)
	{
		EliminationBackoffStack<int> *s = new EliminationBackoffStack<int>(NUM_THREADS, NUM_OPS/NUM_THREADS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<EliminationBackoffStack<int>>, j, NUM_OPS/NUM_THREADS, RATIO_PUSH, s));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		file << MODE << "\t" << RATIO_PUSH << "-" << (100-RATIO_PUSH) << "\t" << NUM_THREADS << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS << "\n";
		delete s;
	}
	else
	{
		std::cout << "Argument 4 not recognized, please retry\n";
		return -1;
	}

	for (int i = 0; i < NUM_OPS; i++)
	{
		//std::cout << "invoked: " << invocations[i] << "\treturned: " << returns[i] << "\tdiff: " << (returns[i]-invocations[i]) << "\n";
	}

}

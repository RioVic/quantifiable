#include "qstackDesc.h"
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

QStackDesc<int> *qD = nullptr;
Treiber_S<int> *treiber = nullptr;
EliminationBackoffStack<int> *ebs = nullptr;

long long **invocations;
long long **returns;
int **pops;
int **pushes;

std::atomic<bool> cutoff;

pthread_barrier_t our_barrier;

inline long long rdtsc() {
  volatile long long tl;
  asm __volatile__("lfence\nrdtsc" : "=a" (tl): : "%edx"); //lfence is used to wait for prior instruction (optional)
  return tl;
}

template<class T>
void work(int thread_id, int num_ops, int push_ratio, T *s)
{
	boost::mt19937 randomGen;
	randomGen.seed(time(0));
	boost::uniform_int<uint32_t> randomDist(1, 1000);

	int popOpn = 0;
	int popThread = 0;
	long long invoked = 0;
	int val = -11;

	for (int i = 0; i < num_ops; i++)
	{
		int r = randomDist(randomGen);
		bool result;
		invoked = rdtsc();

		if ((r % 100) < push_ratio)
		{
			result = s->push(thread_id, i, r, val, popOpn, popThread, invoked);
		}
		else
		{
			int val;
			result = s->pop(thread_id, i, val);
		}
	}
}

//Exports the history of the execution to file based on invocations and returns
//Also pop the stack using a single thread and store the order in which items were popped
template<class T>
void exportHistory(int num_ops, int num_threads, T *s)
{
	std::ofstream f2;
	f2.open(std::string("AMD_") + std::string("Qstack_") + std::to_string(num_threads) + std::string("t_100.dat"), std::ios_base::app);

	//Pop until empty
	//Continue the op counter where the benchmark left off: (total ops/num_thread) == num_ops
	int op = num_ops/num_threads;
	long long invoked;
	long long returned;
	while (!s->isEmpty())
	{
		int val = -11;

		invoked = rdtsc();
		s->pop(0, op, val);
		returned = rdtsc();

		//Sucessful pop
		if (val != -11)
		{
			pops[0][op] = val;
			invocations[0][op] = invoked;
			returns[0][op] = returned;
			op++;
		}
	}

	f2 << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\n";
	for (int i = 0; i < num_ops*2; i++)
	{
		for (int k = 0; k < num_threads; k++)
		{
			if (pops[k][i] == -22 && pushes[k][i] == -22)
				continue;

			long long invoked = invocations[k][i];
			long long returned = returns[k][i];
			int val = (pops[k][i] == -22) ? pushes[k][i] : pops[k][i];

			f2 << "AMD\t" << "QStack\t" << (pops[k][i] == -22 ? "Push\t" : "Pop\t") << k << "\t" << "x" << "\t" << val << "\t" << invoked << "\t" << returned << "\n";
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 5 || strcmp(argv[1],"--help") == 0)
	{
		std::cout << "Please use: " << argv[0] << " <number of threads> <number of operations> <percentage of pushes> <\"QStack\" | \"QStackDesc\" | \"Treiber\" | \"EBS\"> \n";
		return -1;
	}

	int NUM_THREADS = atoi(argv[1]);
	int NUM_OPS = atoi(argv[2]);
	int RATIO_PUSH = atoi(argv[3]);
	char* MODE = argv[4];
	cutoff = false;

	pthread_barrier_init(&our_barrier,NULL,NUM_THREADS);

	std::ofstream file;
	file.open(std::string(MODE) + std::to_string(RATIO_PUSH) + std::string(".dat"), std::ios_base::app);

	std::vector<std::thread> threads;
	if (strcmp(MODE, "Treiber") == 0)
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
		//exportHistory(NUM_OPS, NUM_THREADS, s);
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
		//exportHistory(NUM_OPS, NUM_THREADS, s);
		delete s;
	}
	else if (strcmp(MODE, "QStackDesc") == 0)
	{
		QStackDesc<int> *s = new QStackDesc<int>(NUM_THREADS, NUM_OPS/NUM_THREADS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<QStackDesc<int>>, j, NUM_OPS/NUM_THREADS, RATIO_PUSH, s));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		file << MODE << "\t" << RATIO_PUSH << "-" << (100-RATIO_PUSH) << "\t" << NUM_THREADS << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS << "\n";
		//exportHistory(NUM_OPS, NUM_THREADS, s);
		delete s;
	}
	else
	{
		std::cout << "Argument 4 not recognized, please retry\n";
		return -1;
	}
}

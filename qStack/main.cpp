#include "qstackDesc.h"
#include "ebs.h"
#include "treiber_stack.h"
#include "rdtsc.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <boost/random.hpp>
#include <iostream>
#include <fstream>
#include <time.h>
#include <string>
#include <vector>

struct __attribute__((aligned(64))) timestamp
{
	long long invoked;
	long long returned;
	long long vp; //Visibility Point
	std::string type;
	int val;
	long key;
};

QStackDesc<int> *qD = nullptr;
Treiber_S<int> *treiber = nullptr;
EliminationBackoffStack<int> *ebs = nullptr;

struct timestamp **ts;
std::vector<timestamp> overflowTimestamps;
struct timestamp *idealCaseTimestamps;
int overflowOpCount = 0;

int NUM_THREADS;
int NUM_OPS;
int RATIO_PUSH;
char* MODE;

template<class T>
void work(int thread_id, int num_ops, int push_ratio, T *s, int num_threads)
{
	boost::mt19937 randomGen;
	randomGen.seed(time(0));
	boost::uniform_int<uint32_t> randomDist(1, 1000);

	long long invoked;
	long long returned;
	long long visibilityPoint;
	int insert = thread_id + 1;
	int popOpn;
	int popThread;
	int val;
	bool result;
	int r;

	for (int i = 0; i < num_ops; i++)
	{
		r = randomDist(randomGen);
		val = -11;

		//Log invocation and return, as well as execute a random method
		invoked = rdtsc();

		if ((r % 100) < push_ratio)
		{
			result = s->push(thread_id, i, insert, val, popOpn, popThread, invoked);
			returned = rdtsc();

			//"Push acts as Pop case"
			if (val != -11)
			{
				//This push matched with a pending pop
				//Find the pending pop, update its return timestamp
				//std::cout << "Pending pop found\n" << popOpn << "\t" << popThread << "\t" << insert << "\n";
				ts[popThread][popOpn].returned = returned;
				ts[popThread][popOpn].type = "Pop";
				ts[popThread][popOpn].val = val;
			}
			else
			{
				ts[thread_id][i].returned = returned;
				ts[thread_id][i].type = "Push";
				ts[thread_id][i].val = insert;
			}

			insert += num_threads;
		}
		else
		{
			result = s->pop(thread_id, i, val, visibilityPoint);
			returned = rdtsc();

			//Normal case
			if (val != -11)
			{
				ts[thread_id][i].returned = returned;
				ts[thread_id][i].vp = visibilityPoint;
				ts[thread_id][i].type = "Pop";
				ts[thread_id][i].val = val;
			}

			//If we don't get a value back here, it means the pop is pending, and that we must get the return time when a push() operation satisfies it	
		}

		ts[thread_id][i].invoked = invoked;
		ts[thread_id][i].key = (thread_id) + (NUM_THREADS * i);
	}
}

//Exports the history of the execution to file based on invocations and returns
//Also pop the stack using a single thread and store the order in which items were popped
template<class T>
void exportHistory(int num_ops, int num_threads, T *s)
{
	std::ofstream f2;
	f2.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(NUM_THREADS) + std::string("t_") + std::to_string(RATIO_PUSH) + std::string("_parallel.dat"), std::ios_base::app);

	//Pop until empty
	long long invoked;
	long long returned;
	long long visibilityPoint;
	int op = num_ops++;
	while (!s->isEmpty())
	{
		int val = -11;

		invoked = rdtsc();
		s->pop(0, op, val, visibilityPoint);
		returned = rdtsc();

		//Sucessful pop
		if (val != -11)
		{
			struct timestamp t;

			t.invoked = invoked;
			t.returned = returned;
			t.vp = visibilityPoint;
			t.val = val;
			t.type = "Pop";

			overflowTimestamps.push_back(t);
		}
		op++;
	}

	struct timestamp *operation;

	f2 << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\n";
	for (int i = 0; i < num_ops - 1; i++)
	{
		for (int k = 0; k < num_threads; k++)
		{
			operation = &ts[k][i];

			if (operation->invoked == -1)
			{
				std::cout << "Error, blank timestamp found in history\n";
				exit(EXIT_FAILURE);
			}

			f2 << "AMD\t" << "QStack\t" << operation->type << "\t" << k << "\t" << "x" << "\t" << operation->val << "\t" << operation->invoked << "\t" << operation->returned << "\t" << operation->vp << "\t" << ((operation->type == "Push") ? operation->invoked : operation->vp) << "\n";
		}
	}

	overflowOpCount = (num_ops*num_threads) + overflowTimestamps.size();

	//Print remaining overflow ops
	for (int i = 0; i < overflowTimestamps.size(); i++)
	{
		operation = &overflowTimestamps[i];

			f2 << "AMD\t" << "QStack\t" << operation->type << "\t" << "0" << "\t" << "x" << "\t" << operation->val << "\t" << operation->invoked << "\t" << operation->returned << "\t" << operation->vp << "\t" << ((operation->type == "Push") ? operation->invoked : operation->vp) << "\n";
	}
}

bool compareTimestamp(timestamp t1, timestamp t2)
{
	//long long t1Time = (t1.type == "Push") ? t1.invoked : t1.vp;
	//long long t2Time = (t2.type == "Push") ? t2.invoked : t2.vp;

	//return (t1Time < t2Time);

	return (t1.invoked < t2.invoked);
}

//Repeats the parallel history on a single thread and returns the "ideal case" history
template<class T>
void executeHistorySequentially(int num_ops_total, int num_threads, T *s)
{
	//Merge timestamp arrays into vector and sort
	std::vector<timestamp> history;

	for (int i = 0; i < num_ops_total/num_threads; i++)
	{
		for (int k = 0; k < num_threads; k++)
		{
			history.push_back(ts[k][i]);
		}
	}

	//Print remaining overflow ops
	for (int i = 0; i < overflowTimestamps.size(); i++)
	{
		history.push_back(overflowTimestamps[i]);
	}
	std::sort(history.begin(), history.end(), compareTimestamp);
	idealCaseTimestamps = new timestamp[history.size()];

	//Run the test sequentially and record the results
	long long invoked;
	long long returned;
	int popOpn;
	int popThread;
	int val;
	bool result;
	int r;
	
	for (int i = 0; i < history.size(); i++)
	{
		val = -11;

		invoked = rdtsc();
		if (history[i].type == "Push")
		{
			result = s->push(0, i, history[i].val, val, popOpn, popThread, invoked);
			returned = rdtsc();

			//Push acts as pop case
			if (val != -11)
			{
				idealCaseTimestamps[popOpn].returned = returned;
				idealCaseTimestamps[popOpn].type = "Pop";
				idealCaseTimestamps[popOpn].val = val;
			}
			else //Normal case
			{
				idealCaseTimestamps[i].returned = returned;
				idealCaseTimestamps[i].type = "Push";
				idealCaseTimestamps[i].val = history[i].val;
			}
		}
		else if (history[i].type == "Pop")
		{
			long long vp;
			result = s->pop(0, i, val, vp);
			returned = rdtsc();

			//Normal case
			if (val != 11)
			{
				idealCaseTimestamps[i].returned = returned;
				idealCaseTimestamps[i].vp = vp;
				idealCaseTimestamps[i].type = "Pop";
				idealCaseTimestamps[i].val = val;
			}
		}
		else
		{
			std::cout << "Error: timestamp found without type\n";
			exit(EXIT_FAILURE);
		}

		idealCaseTimestamps[i].invoked = invoked;
	}

	//Export results to file
	std::ofstream f2;
	f2.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(NUM_THREADS) + std::string("t_") + std::to_string(RATIO_PUSH) + std::string("_ideal.dat"), std::ios_base::app);

	struct timestamp *operation;

	f2 << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\n";
	for (int i = 0; i < history.size(); i++)
	{
		operation = &idealCaseTimestamps[i];

		if (operation->invoked == -1)
		{
			std::cout << "Error, blank timestamp found in history\n";
			exit(EXIT_FAILURE);
		}

		f2 << "AMD\t" << "QStack\t" << operation->type << "\t" << "0" << "\t" << "x" << "\t" << operation->val << "\t" << operation->invoked << "\t" << operation->returned << "\t" << operation->vp << "\t" << ((operation->type == "Push") ? operation->invoked : operation->vp) << "\n";
	}
}

int main(int argc, char** argv)
{
	if (argc < 5 || strcmp(argv[1],"--help") == 0)
	{
		std::cout << "Please use: " << argv[0] << " <number of threads> <number of operations> <percentage of pushes> <\"QStack\" | \"QStackDesc\" | \"Treiber\" | \"EBS\"> \n";
		return -1;
	}

	NUM_THREADS = atoi(argv[1]);
	NUM_OPS = atoi(argv[2]);
	RATIO_PUSH = atoi(argv[3]);
	MODE = argv[4];

	ts = new timestamp*[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++)
	{
		ts[i] = new timestamp[NUM_OPS/NUM_THREADS];
	}

	std::ofstream file;
	file.open(std::string(MODE) + std::to_string(RATIO_PUSH) + std::string(".dat"), std::ios_base::app);

	std::vector<std::thread> threads;
	if (strcmp(MODE, "Treiber") == 0)
	{
		Treiber_S<int> *s = new Treiber_S<int>(NUM_THREADS, NUM_OPS/NUM_THREADS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<Treiber_S<int>>, j, NUM_OPS/NUM_THREADS, RATIO_PUSH, s, NUM_THREADS));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		file << MODE << "\t" << RATIO_PUSH << "-" << (100-RATIO_PUSH) << "\t" << NUM_THREADS << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS << "\n";
		exportHistory(NUM_OPS/NUM_THREADS, NUM_THREADS, s);
		delete s;

		s = new Treiber_S<int>(1, overflowOpCount);
		executeHistorySequentially(NUM_OPS, NUM_THREADS, s);
	}
	else if (strcmp(MODE, "EBS") == 0)
	{
		EliminationBackoffStack<int> *s = new EliminationBackoffStack<int>(NUM_THREADS, NUM_OPS/NUM_THREADS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<EliminationBackoffStack<int>>, j, NUM_OPS/NUM_THREADS, RATIO_PUSH, s, NUM_THREADS));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		file << MODE << "\t" << RATIO_PUSH << "-" << (100-RATIO_PUSH) << "\t" << NUM_THREADS << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS << "\n";
		exportHistory(NUM_OPS/NUM_THREADS, NUM_THREADS, s);
		delete s;

		s = new EliminationBackoffStack<int>(1, overflowOpCount);
		executeHistorySequentially(NUM_OPS, NUM_THREADS, s);
	}
	else if (strcmp(MODE, "QStackDesc") == 0)
	{
		QStackDesc<int> *s = new QStackDesc<int>(NUM_THREADS, NUM_OPS/NUM_THREADS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<QStackDesc<int>>, j, NUM_OPS/NUM_THREADS, RATIO_PUSH, s, NUM_THREADS));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		file << MODE << "\t" << RATIO_PUSH << "-" << (100-RATIO_PUSH) << "\t" << NUM_THREADS << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\t" << NUM_OPS << "\n";
		exportHistory(NUM_OPS/NUM_THREADS, NUM_THREADS, s);
		delete s;

		s = new QStackDesc<int>(1, overflowOpCount);
		executeHistorySequentially(NUM_OPS, NUM_THREADS, s);
	}
	else
	{
		std::cout << "Argument 4 not recognized, please retry\n";
		return -1;
	}
}

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

void logOp(struct timestamp &ts, long long vp, std::string type, int val)
{
	ts.vp = vp;
	ts.type = type;
	ts.val = val;
}

void logOp(struct timestamp &ts, long long invoked, long key)
{
	ts.invoked = invoked;
	ts.key = key;
}

void writeToFile(std::ofstream &f, int thread_id, int num_ops, struct timestamp *ts)
{
	struct timestamp *operation;

	for (int i = 0; i < num_ops - 1; i++)
	{
		operation = &ts[i];

		if (operation->invoked == -1)
		{
			std::cout << "Error, blank timestamp found in history\n";
			exit(EXIT_FAILURE);
		}

		f << "AMD\t" << MODE << "\t" << operation->type << "\t" << thread_id << "\t" << "x" << "\t" << operation->val << "\t" << operation->invoked << "\t" << operation->returned << "\t" << operation->vp << "\t" << ((operation->type == "Push") ? operation->invoked : operation->vp) << "\t" << operation->key << "\n";
	}
}

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
	int key;

	for (int i = 0; i < num_ops; i++)
	{
		r = randomDist(randomGen);
		val = -11;
		key = (thread_id) + (NUM_THREADS * i);

		//Log invocation
		invoked = rdtsc();

		if ((r % 100) < push_ratio)
		{
			result = s->push(thread_id, i, insert, val, popOpn, popThread, invoked);

			//"Push acts as Pop case"
			if (val != -11)
				logOp(ts[popThread][popOpn], -1, "Pop", val);
			else
				logOp(ts[thread_id][i], -1, "Push", insert);

			insert += num_threads;
		}
		else
		{
			result = s->pop(thread_id, i, val, visibilityPoint);

			//Normal case
			if (val != -11)
				logOp(ts[thread_id][i], visibilityPoint, "Pop", val);

			//If we don't get a value back here, it means the pop is pending, and that we must get the return time when a push() operation satisfies it	
		}

		//Log shared fields (invocation and key)
		logOp(ts[thread_id][i], invoked, key);
	}
}

//Exports the history of the execution to file based on invocations and returns
//Also pop the stack using a single thread and store the order in which items were popped
template<class T>
void exportHistory(int num_ops, int num_threads, T *s)
{
	std::ofstream parallelF;
	parallelF.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(NUM_THREADS) + std::string("t_") + std::to_string(RATIO_PUSH) + std::string("_parallel.dat"), std::ios_base::app);

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

	parallelF << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\tkey\n";
	for (int k = 0; k < num_threads; k++)
	{
		writeToFile(parallelF, k, num_ops, ts[k]);
	}

	struct timestamp arr[overflowTimestamps.size()];
	std::copy(overflowTimestamps.begin(), overflowTimestamps.end(), arr);
	writeToFile(parallelF, 0, overflowTimestamps.size(), arr);
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

			//Push acts as pop case
			if (val != -11)
				logOp(idealCaseTimestamps[popOpn], -1, "Pop", val);
			else //Normal case
				logOp(idealCaseTimestamps[popOpn], -1, "Push", history[i].val);
		}
		else if (history[i].type == "Pop")
		{
			long long vp;
			result = s->pop(0, i, val, vp);

			//Normal case
			if (val != 11)
				logOp(idealCaseTimestamps[popOpn], vp, "Pop", val);
		}
		else
		{
			std::cout << "Error: timestamp found without type\n";
			exit(EXIT_FAILURE);
		}

		//Log shared fields (invocation and key)
		logOp(idealCaseTimestamps[popOpn], invoked, history[i].key);
	}

	//Export results to file
	std::ofstream idealF;
	idealF.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(NUM_THREADS) + std::string("t_") + std::to_string(RATIO_PUSH) + std::string("_ideal.dat"), std::ios_base::app);
	idealF << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\tkey\n";

	writeToFile(idealF, 0, history.size(), idealCaseTimestamps);
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

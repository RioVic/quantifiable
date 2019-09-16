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
	long long returned = -1;
	long long vp = -1; //Visibility Point
	std::string type;
	int val;
	long key;
	int threadId;
};

QStackDesc<int> *qD = nullptr;
Treiber_S<int> *treiber = nullptr;
EliminationBackoffStack<int> *ebs = nullptr;

struct timestamp **ts;
struct timestamp *history;
int allOpCount = 0;

int NUM_THREADS;
int NUM_OPS;
int PAIRWISE_INTERVAL;
int THREAD_INTERVAL;
int PAIRWISE_SETS;
char* MODE;

pthread_barrier_t workBarrier;

void logOp(struct timestamp &ts, std::string type, int val)
{
	ts.type = type;
	ts.val = val;
}

void logOp(struct timestamp &ts, long long invoked, long key, int threadId)
{
	ts.invoked = invoked;
	ts.key = key;
	ts.threadId = threadId;
}

void writeToFile(std::ofstream &f, int num_ops, struct timestamp *ts)
{
	struct timestamp *operation;

	for (int i = 0; i < num_ops; i++)
	{
		operation = &ts[i];

		if (operation->invoked == -1)
		{
			std::cout << "Error, blank timestamp found in history, id: " << operation->threadId << "\top: " << i << "\n";
			exit(EXIT_FAILURE);
		}

		f << "AMD\t" << MODE << "\t" << operation->type << "\t" << operation->threadId << "\t" << "x" << "\t" << operation->val << "\t" << operation->invoked << "\t" << operation->returned << "\t" << operation->vp << "\t" << ((operation->type == "Push") ? operation->invoked : operation->vp) << "\t" << operation->key << "\n";
	}
}

bool compareTimestamp(timestamp t1, timestamp t2)
{
	//long long t1Time = (t1.type == "Push") ? t1.invoked : t1.vp;
	//long long t2Time = (t2.type == "Push") ? t2.invoked : t2.vp;

	//return (t1Time < t2Time);

	return (t1.invoked < t2.invoked);
}

template<class T>
void work(int thread_id, T *s, int num_threads)
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
	int threadOpNum;

	pthread_barrier_wait(&workBarrier);

	//Perform THREAD_INTERVAL pushes followed by THREAD_INTERVAL pops
	for (int i = 0; i < PAIRWISE_SETS; i++)
	{
		for (int k = 0; k < THREAD_INTERVAL * 2; k++)
		{
			r = randomDist(randomGen);
			val = -11;
			key = (thread_id) + (NUM_THREADS * (k+(THREAD_INTERVAL*2*i)));
			threadOpNum = (k+(THREAD_INTERVAL*2*i));

			//Log invocation
			invoked = rdtsc();

			//if (k == THREAD_INTERVAL)
				//pthread_barrier_wait(&workBarrier);

			if (k < THREAD_INTERVAL)
			{
				result = s->push(thread_id, threadOpNum, insert, val, popOpn, popThread);

				//"Push acts as Pop case"
				if (val != -11)
					logOp(ts[popThread][popOpn], "Pop", val);
				else
					logOp(ts[thread_id][threadOpNum], "Push", insert);

				insert += num_threads;
			}
			else
			{
				result = s->pop(thread_id, threadOpNum, val);

				//Normal case
				if (val != -11)
					logOp(ts[thread_id][threadOpNum], "Pop", val);

				//If we don't get a value back here, it means the pop is pending, and that we must get the return time when a push() operation satisfies it	
			}

			//Log shared fields (invocation and key)
			logOp(ts[thread_id][threadOpNum], invoked, key, thread_id);
		}
	}
}

//Exports the history of the execution to file based on invocations and returns
//Also pop the stack using a single thread and store the order in which items were popped
template<class T>
void exportHistory(int num_threads, T *s)
{
	std::ofstream parallelF;
	parallelF.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(NUM_THREADS) + std::string("t_") + std::to_string(PAIRWISE_INTERVAL) + std::string("i_") + std::to_string(PAIRWISE_SETS) + std::string("_parallel.dat"), std::ios_base::app);

	//Pop until empty
	std::vector<timestamp> overflowTimestamps;
	long long invoked;
	long long returned;
	long long visibilityPoint;
	int op = 0;
	while (!s->isEmpty())
	{
		int val = -11;

		invoked = rdtsc();
		s->pop(0, op, val);
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
			t.key = NUM_OPS + op;

			overflowTimestamps.push_back(t);
		}
		op++;
	}

	//Merge all timestamps arrays into vector and sort
	allOpCount = (NUM_OPS*num_threads) + overflowTimestamps.size();
	history = new timestamp[allOpCount];
	int index = 0;

	for (int i = 0; i < NUM_OPS; i++)
	{
		for (int k = 0; k < num_threads; k++)
		{
			history[index++] = ts[k][i];
		}
	}

	for (int i = 0; i < overflowTimestamps.size(); i++)
	{
		history[index++] = overflowTimestamps[i];
	}

	std::sort(history, history+allOpCount, compareTimestamp);
	parallelF << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\tkey\n";
	writeToFile(parallelF, allOpCount, history);
}

//Repeats the parallel history on a single thread and returns the "ideal case" history
template<class T>
void executeHistorySequentially(int num_ops_total, int num_threads, T *s)
{
	struct timestamp *idealCaseTimestamps = new timestamp[allOpCount];

	//Run the test sequentially and record the results
	long long invoked;
	long long returned;
	int popOpn;
	int popThread;
	int val;
	bool result;
	int r;

	for (int i = 0; i < allOpCount; i++)
	{
		val = -11;

		invoked = rdtsc();
		if (history[i].type == "Push")
		{
			result = s->push(0, i, history[i].val, val, popOpn, popThread);

			//Push acts as pop case
			if (val != -11)
				logOp(idealCaseTimestamps[popOpn], "Pop", val);
			else //Normal case
				logOp(idealCaseTimestamps[i], "Push", history[i].val);
		}
		else if (history[i].type == "Pop")
		{
			long long vp;
			result = s->pop(0, i, val);

			//Normal case
			if (val != -11)
				logOp(idealCaseTimestamps[i], "Pop", val);
		}
		else
		{
			std::cout << "Error: timestamp found without type: " << i << "\n";
			exit(EXIT_FAILURE);
		}

		//Log shared fields (invocation and key)
		logOp(idealCaseTimestamps[i], invoked, history[i].key, 0);
	}

	//Export results to file
	std::ofstream idealF;
	idealF.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(NUM_THREADS) + std::string("t_") + std::to_string(PAIRWISE_INTERVAL) + std::string("i_") + std::to_string(PAIRWISE_SETS) + std::string("_ideal.dat"), std::ios_base::app);
	idealF << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\tkey\n";

	writeToFile(idealF, allOpCount, idealCaseTimestamps);
}

int main(int argc, char** argv)
{
	if (argc < 5 || strcmp(argv[1],"--help") == 0)
	{
		std::cout << "Please use: " << argv[0] << " <number of threads> <size of operation intervals> <number of intervals> <\"QStackDesc\" | \"Treiber\" | \"EBS\"> \n";
		return -1;
	}

	NUM_THREADS = atoi(argv[1]);
	PAIRWISE_INTERVAL = atoi(argv[2]);
	PAIRWISE_SETS = atoi(argv[3]);
	MODE = argv[4];

	THREAD_INTERVAL = PAIRWISE_INTERVAL/NUM_THREADS;
	NUM_OPS = THREAD_INTERVAL*2*PAIRWISE_SETS;

	pthread_barrier_init(&workBarrier,NULL,NUM_THREADS);

	ts = new timestamp*[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++)
	{
		ts[i] = new timestamp[NUM_OPS];
	}

	std::vector<std::thread> threads;
	if (strcmp(MODE, "Treiber") == 0)
	{
		Treiber_S<int> *s = new Treiber_S<int>(NUM_THREADS, NUM_OPS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<Treiber_S<int>>, j, s, NUM_THREADS));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		exportHistory(NUM_THREADS, s);
		delete s;

		s = new Treiber_S<int>(1, allOpCount);
		executeHistorySequentially(NUM_OPS, NUM_THREADS, s);
	}
	else if (strcmp(MODE, "EBS") == 0)
	{
		EliminationBackoffStack<int> *s = new EliminationBackoffStack<int>(NUM_THREADS, NUM_OPS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<EliminationBackoffStack<int>>, j, s, NUM_THREADS));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		exportHistory(NUM_THREADS, s);
		delete s;

		s = new EliminationBackoffStack<int>(1, allOpCount);
		executeHistorySequentially(NUM_OPS, NUM_THREADS, s);
	}
	else if (strcmp(MODE, "QStackDesc") == 0)
	{
		QStackDesc<int> *s = new QStackDesc<int>(NUM_THREADS, NUM_OPS);
		auto start = std::chrono::high_resolution_clock::now();

		for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&work<QStackDesc<int>>, j, s, NUM_THREADS));

		for (std::thread &t : threads)
			t.join();

		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = end-start;

		exportHistory(NUM_THREADS, s);
		delete s;

		s = new QStackDesc<int>(1, allOpCount);
		executeHistorySequentially(NUM_OPS, NUM_THREADS, s);
	}
	else
	{
		std::cout << "Argument 4 not recognized, please retry\n";
		return -1;
	}
}

#include <thread>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <boost/random.hpp>
#include <fstream>
#include <time.h>
#include <string>
#include <vector>
#include "unboundedsize_kfifo.h"
#include "./util/rdtsc.h"

int queue_param_k;

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

scal::UnboundedSizeKFifo<long unsigned int> *q = nullptr;

struct timestamp **ts;
struct timestamp *history;
int allOpCount = 0;

int NUM_THREADS;
int NUM_OPS;
int PAIRWISE_INTERVAL;
int THREAD_INTERVAL;
int PAIRWISE_SETS;
std::string MODE;

pthread_barrier_t workBarrier;
std::string s1 ("1g"); //TODO 1k or 1m or 1g
size_t tlsize;

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

void work(int thread_id, int num_threads)
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
	long unsigned int val;
	bool result;
	int r;
	int key;
	int threadOpNum;

    scal::ThreadLocalAllocator::Get().Init(tlsize, true);

	pthread_barrier_wait(&workBarrier);

	//Perform THREAD_INTERVAL pushes followed by THREAD_INTERVAL pops
	for (int i = 0; i < PAIRWISE_SETS; i++)
	{
		for (int k = 0; k < THREAD_INTERVAL * 2; k++)
		{
			r = randomDist(randomGen);
			key = (thread_id) + (NUM_THREADS * (k+(THREAD_INTERVAL*2*i)));
			threadOpNum = (k+(THREAD_INTERVAL*2*i));

			//Log invocation
			invoked = rdtsc();

			if (k < THREAD_INTERVAL)
			{
				result = q->enqueue((long unsigned int) insert);
				logOp(ts[thread_id][threadOpNum], "Enqueue", insert);
				insert += num_threads;
			}
			else
			{
				result = q->dequeue(&val);

				//Normal case
				if (result)
					logOp(ts[thread_id][threadOpNum], "Dequeue", val);
			}

			//Log shared fields (invocation and key)
			logOp(ts[thread_id][threadOpNum], invoked, key, thread_id);
		}
	}
}

//Exports the history of the execution to file based on invocations and returns
//Also pop the stack using a single thread and store the order in which items were popped
void exportHistory(int num_threads)
{
	std::ofstream parallelF;
	parallelF.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(queue_param_k) + std::string("k_") + std::to_string(NUM_THREADS) + std::string("t_") + std::to_string(PAIRWISE_INTERVAL) + std::string("i_") + std::to_string(PAIRWISE_SETS) + std::string("_parallel.dat"), std::ios_base::app);

	//Merge all timestamps arrays into vector and sort
	allOpCount = (NUM_OPS*num_threads);
	history = new timestamp[allOpCount];
	int index = 0;

	for (int i = 0; i < NUM_OPS; i++)
	{
		for (int k = 0; k < num_threads; k++)
		{
			history[index++] = ts[k][i];
		}
	}

	std::sort(history, history+allOpCount, compareTimestamp);
	parallelF << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\tkey\n";
	writeToFile(parallelF, allOpCount, history);
}

//Repeats the parallel history on a single thread and returns the "ideal case" history
void executeHistorySequentially(int num_ops_total, int num_threads)
{
	struct timestamp *idealCaseTimestamps = new timestamp[allOpCount];

	//Run the test sequentially and record the results
	long long invoked;
	long long returned;
	int popOpn;
	int popThread;
	long unsigned int val;
	bool result;
	int r;

	for (int i = 0; i < allOpCount; i++)
	{
		invoked = rdtsc();
		if (history[i].type == "Enqueue")
		{
			result = q->enqueue((long unsigned int) history[i].val);
			logOp(idealCaseTimestamps[i], "Enqueue", history[i].val);
		}
		else if (history[i].type == "Dequeue")
		{
            //val = new long unsigned int;
			result = q->dequeue(&val);

            //Normal case
            if (result)
                logOp(idealCaseTimestamps[i], "Dequeue", val);
		}
		else
		{
			std::cout << "Error: timestamp found without type\n";
			exit(EXIT_FAILURE);
		}

		//Log shared fields (invocation and key)
		logOp(idealCaseTimestamps[i], invoked, history[i].key, 0);
	}

	//Export results to file
	std::ofstream idealF;
	idealF.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(queue_param_k) + std::string("k_") + std::to_string(NUM_THREADS) + std::string("t_") + std::to_string(PAIRWISE_INTERVAL) + std::string("i_") + std::to_string(PAIRWISE_SETS) + std::string("_ideal.dat"), std::ios_base::app);
	idealF << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tvisibilityPoint\tprimaryStamp\tkey\n";

	writeToFile(idealF, allOpCount, idealCaseTimestamps);
}

int main(int argc, char** argv)
{
	if (argc < 5 || strcmp(argv[1],"--help") == 0)
	{
		std::cout << "Please use: " << argv[0] << " <number of threads> <size of operation intervals> <number of intervals> <k>\n";
		return -1;
	}

	NUM_THREADS = atoi(argv[1]);
	PAIRWISE_INTERVAL = atoi(argv[2]);
	PAIRWISE_SETS = atoi(argv[3]);
	MODE = std::string("KFifoQueue");
	queue_param_k = atoi(argv[4]);

	THREAD_INTERVAL = PAIRWISE_INTERVAL/NUM_THREADS;
	NUM_OPS = THREAD_INTERVAL*2*PAIRWISE_SETS;

    //Allocation Setup
    tlsize = scal::HumanSizeToPages(s1.c_str(), s1.size());
    scal::ThreadLocalAllocator::Get().Init(tlsize, true);

	pthread_barrier_init(&workBarrier,NULL,NUM_THREADS);

	ts = new timestamp*[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++)
	{
		ts[i] = new timestamp[NUM_OPS];
	}

    //Spawn threads and send them into work loop
    std::vector<std::thread> threads;
    q = new scal::UnboundedSizeKFifo<long unsigned int>(queue_param_k);
    auto start = std::chrono::high_resolution_clock::now();

    for (int j = 0; j < NUM_THREADS; j++)
        threads.push_back(std::thread(work, j, NUM_THREADS));

    for (std::thread &t : threads)
        t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end-start;

    exportHistory(NUM_THREADS);
    delete q;

    q = new scal::UnboundedSizeKFifo<long unsigned int>(queue_param_k);
    executeHistorySequentially(NUM_OPS, NUM_THREADS);

}

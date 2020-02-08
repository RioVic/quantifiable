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

scal::UnboundedSizeKFifo<long unsigned int> *q = nullptr;

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

			if (k < THREAD_INTERVAL)
			{
				result = q->enqueue((long unsigned int) insert);
				insert += num_threads;
			}
			else
			{
				result = q->dequeue(&val);
			}
		}
	}
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

	std::ofstream file;
    file.open(std::string("AMD_") + MODE + std::string("_") + std::to_string(queue_param_k) + std::string("k_") + std::to_string(PAIRWISE_INTERVAL) + std::string("i_") + std::to_string(PAIRWISE_SETS) + std::string(".dat"), std::ios_base::app);

	THREAD_INTERVAL = PAIRWISE_INTERVAL/NUM_THREADS;
	NUM_OPS = THREAD_INTERVAL*2*PAIRWISE_SETS;

    //Allocation Setup
    tlsize = scal::HumanSizeToPages(s1.c_str(), s1.size());
    scal::ThreadLocalAllocator::Get().Init(tlsize, true);

	pthread_barrier_init(&workBarrier,NULL,NUM_THREADS);

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

	file << MODE << "\t" << PAIRWISE_INTERVAL*2*PAIRWISE_SETS << "\t" << NUM_THREADS << "\t" << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "\n";

    //exportHistory(NUM_THREADS);
    //delete q;

    //q = new scal::UnboundedSizeKFifo<long unsigned int>(queue_param_k);
    //executeHistorySequentially(NUM_OPS, NUM_THREADS);

}


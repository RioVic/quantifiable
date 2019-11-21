#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <thread>
#include <map>
#include <string.h>

#define MAX_PERIOD_LEN 64
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

using namespace std;

struct __attribute__((aligned(64))) operation 
{
	long key;
	long long timestamp;
	int order;
	int val;
	string name;
	int inversion = -1;
	int proc;
	int result;
};

std::vector<operation> pCase;
std::vector<operation> iCase;
int NUM_THREADS;
string benchName;

pthread_barrier_t workBarrier;

bool compareOperation(operation t1, operation t2)
{
	return (t1.timestamp < t2.timestamp);
}

//Reads the history file into a vector and sorts it by timestamp
void readHistory(std::ifstream &f, std::vector<operation> &v)
{
	string line;
	getline(f, line); //Skip first line
	int i = 0;

	while (getline(f, line))
	{
		std::stringstream ss(line);
		std::vector<std::string> lineElems;
		while (getline(ss, line, '\t'))
		{
			lineElems.push_back(line);
		}

		if (benchName.empty())
			benchName = lineElems[1];

		struct operation op;
		op.val = stoi(lineElems[5]);
		op.timestamp = stoll(lineElems[6]);
		op.name = lineElems[2];
		op.proc = stoi(lineElems[3]);
		op.result = stoi(lineElems[4]);
		op.key = stoi(lineElems[10]);
		v.push_back(op);
		i++;
	}

	std::sort(v.begin(), v.end(), compareOperation);
}

int findWhenContained(int index, int range, int val, int key)
{
	for (int i = 0; i < range; i++)
	{
		int left = index - i;
		int right = index + i;

		//Search backwards in time for a remove
		if (left > 0)
		{
			if (pCase[left].val == val && pCase[left].name == "REMOVE" && pCase[left].result == 1)
				return i;
		}

		//Search forwards in time for an add
		if (right < range)
		{
			if (pCase[right].val == val && pCase[right].name == "ADD")
				return i;
		}
	}

	std::cout << "Error: should not happen\n";
}

int findWhenNotContained(int index, int range, int val, int key)
{
	for (int i = 0; i < range; i++)
	{
		int left = index - i;
		int right = index + i;

		//Search backwards in time for an add
		if (left > 0)
		{
			if (pCase[left].val == val && pCase[left].name == "ADD" && pCase[left].result == 1)
				return i;
		}

		//Search forwards in time for a remove
		if (right < range)
		{
			if (pCase[right].val == val && pCase[right].name == "REMOVE")
				return i;
		}
	}

	std::cout << "Error: should not happen\n";
}

void setOrder(string filename, int id)
{
	int findIndex;

	// build index of FIRST occurence of key
	map<int, int> key2pCaseI;
	map<int, int> key2iCaseI;
	for (int i = 0; i < pCase.size(); i++) {
		if (key2pCaseI.count(pCase[i].key) == 0) {
			key2pCaseI[pCase[i].key] = i;
		}
	}
	for (int i = 0; i < iCase.size(); i++) {
		if (key2iCaseI.count(iCase[i].key) == 0) {
			key2iCaseI[iCase[i].key] = i;
		}
	}

	for (int i = id; i < iCase.size(); i+=NUM_THREADS)
	{
		iCase[i].order = i;

		if (i % 25000 == 0)
			std::cout << "Calculating item number: " << i << " ...\n";

		int k = key2pCaseI[iCase[i].key];

		if (pCase[k].key == 1)
		{
			std::cout << pCase[k].result << "\t" << iCase[i].result << "\n";
		}
		if (pCase[k].result == iCase[i].result)
		{
			pCase[k].inversion = 0;
			if (pCase[k].key == 1)
			{
				std::cout << pCase[k].result << "\t" << iCase[i].result << "\n";
				std::cout << pCase[k].name << "\t" << pCase[k].val << "\n";
			}
		}
		else
		{
			std::cout << pCase[k].name << "\n";
			if (pCase[k].name == "REMOVE" || pCase[k].name == "CONTAINS")
			{
				if (iCase[i].result == 1)
				{
					pCase[k].inversion = findWhenContained(k, pCase.size(), pCase[k].val, pCase[k].key);
				}
				else if (iCase[i].result == 0)
				{
					pCase[k].inversion = findWhenNotContained(k, pCase.size(), pCase[k].val, pCase[k].key);
				}
			}
			else if (pCase[k].name == "ADD")
			{
				if (iCase[i].result == 1)
				{
					pCase[k].inversion = findWhenNotContained(k, pCase.size(), pCase[k].val, pCase[k].key);
				}
				else if (iCase[i].result == 0)
				{
					pCase[k].inversion = findWhenContained(k, pCase.size(), pCase[k].val, pCase[k].key);
				}
			}
		}
	}

    pthread_barrier_wait(&workBarrier);

	if (id != 0)
		return;

	std::ofstream ofs;
	ofs.open(filename);

	ofs << "arch\talgo\tmethod\tproc\tobject\titem\tinvoke\tfinish\tstart_order\tmethod2\titem2\tinvoke2\tfinish2\tfinish_order\tinversion\n";

	if (ofs.is_open())
	{
		for (int i = 0; i < pCase.size(); i++)
		{

			int k = key2iCaseI[pCase[i].key];

			ofs << "AMD\t" << benchName << "\t" << iCase[k].name << "\t" << iCase[k].proc << "\t" << iCase[k].result << "\t" << iCase[k].val << "\t" << iCase[k].timestamp << "\t" << "0\t" 
			<< iCase[k].order << "\t" << pCase[i].name << "\t" << pCase[i].val << "\t" << pCase[i].timestamp << "\t" << "0\t" << i << "\t" << pCase[i].inversion << "\n";

			//ofs << pCase[i].order << "\t" << pCase[i].name << "\t" << pCase[i].val << "\t" << pCase[i].inversion << "\n";
			//break;
		}
	}
}

void compareKeys(std::vector<operation> &pCase, std::vector<operation> &iCase)
{
	return;
}

int main(int argc, char** argv)
{
	if (argc != 4)
	{
		std::cout << "Please use: " << argv[0] << " <Entropy Data File 1 (Parallel Case)> <Entropy Data File 2 (Ideal Case)> <Number Of Threads>\n";
		return -1;
	}

	std::ifstream f1;
	std::ifstream f2;
	NUM_THREADS = atoi(argv[3]);
	NUM_THREADS = 1;

	pthread_barrier_init(&workBarrier,NULL,NUM_THREADS);

	f1.open(argv[1]);
	f2.open(argv[2]);

	if (!f1.is_open() || !f2.is_open())
	{
		std::cout << "Error opening file\n";
		exit(EXIT_FAILURE);
	}

	string filename = argv[1];
	size_t pos = filename.find("_parallel.dat");
	filename.erase(pos, 13);
	filename = filename + "_inversions.dat";

	readHistory(f1, pCase);
	readHistory(f2, iCase);

	f1.close();
	f2.close();

	std::vector<std::thread> threads;

	for (int j = 0; j < NUM_THREADS; j++)
			threads.push_back(std::thread(&setOrder, filename, j));

	for (std::thread &t : threads)
			t.join();
}

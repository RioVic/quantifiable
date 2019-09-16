#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <thread>

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

		if (lineElems[2] == "Enqueue")
		{
			continue;
		}

		if (benchName.empty())
			benchName = lineElems[1];

		struct operation op;
		op.val = stoi(lineElems[5]);
		op.timestamp = stoll(lineElems[6]);
		op.name = lineElems[2];
		op.proc = stoi(lineElems[3]);
		v.push_back(op);
		i++;
	}

	std::sort(v.begin(), v.end(), compareOperation);
}

void setOrder(string filename, int id)
{
	for (int i = id; i < iCase.size(); i+=NUM_THREADS)
	{
		iCase[i].order = i;

		if (i % 25000 == 0)
			std::cout << "Logging item number: " << i << " ...\n";

		for (int k = 0; k < pCase.size(); k++)
		{
			if (pCase[k].val == iCase[i].val)
			{
				pCase[k].order = i;
				break;
			}
		}
	}

	pthread_barrier_wait(&workBarrier);

	for (int i = id; i < pCase.size(); i+=NUM_THREADS)
	{
		int currentOpOrder = pCase[i].order;
		int inversion = 0;

		if (i % 25000 == 0)
			std::cout << "Processing item number: " << i << " ...\n";

		for (int k = 0; k < pCase.size(); k++)
		{
			if (k < i && pCase[k].order > currentOpOrder)
				inversion++;
			else if (k >= i)
				break;
		}

		pCase[i].inversion = inversion;
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
			for (int k = 0; k < iCase.size(); k++)
			{
				if (pCase[i].val == iCase[k].val)
				{
					ofs << "AMD\t" << benchName << "\t" << iCase[k].name << "\t" << iCase[k].proc << "\t" << "x\t" << iCase[k].val << "\t" << iCase[k].timestamp << "\t" << "0\t" 
					<< iCase[k].order << "\t" << pCase[i].name << "\t" << pCase[i].val << "\t" << pCase[i].timestamp << "\t" << "0\t" << i << "\t" << pCase[i].inversion << "\n";

					break;
				}
			}
			
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
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <algorithm>

using namespace std;

struct operation
{
	long key;
	long long timestamp;
	int order;
	int val;
};

std::vector<operation> parallelCase;
std::vector<operation> idealCase;

bool compareOperation(operation t1, operation t2)
{
	return (t1.timestamp < t2.timestamp);
}

//Reads the history file into a vector and sorts it by timestamp
void readHistory(std::ifstream &f, std::vector<operation> &v)
{
	string line;
	getline(f, line); //Skip first line

	while (getline(f, line))
	{
		std::stringstream ss(line);
		std::vector<std::string> lineElems;
		while (getline(ss, line, '\t'))
		{
			lineElems.push_back(line);
		}

		if (lineElems[2] == "Push")
		{
			continue;
		}

		struct operation op;
		op.val = stoi(lineElems[5]);
		op.timestamp = stoi(lineElems[9]);
		v.push_back(op);
	}

	std::sort(v.begin(), v.end(), compareOperation);
}

void setOrder(std::vector<operation> &pCase, std::vector<operation> &iCase)
{
	for (int i = 0; i < iCase.size(); i++)
	{
		iCase[i].order = i;

		for (int k = 0; k < pCase.size(); k++)
		{
			if (pCase[k].val == iCase[i].val)
			{
				pCase[i].order = k;
			}
		}
	}

	/*for (int i = 0; i < iCase.size(); i++)
	{
		std::cout << "Order: " << pCase[i].order << "\t Val: " << pCase[i].val << "\n";
	}*/
}

void compareKeys(std::vector<operation> &pCase, std::vector<operation> &iCase)
{
	return;
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cout << "Please use: " << argv[0] << " <Entropy Data File 1 (Parallel Case)> <Entropy Data File 2 (Ideal Case)>\n";
	}

	std::ifstream f1;
	std::ifstream f2;

	f1.open(argv[1]);
	f2.open(argv[2]);

	if (!f1.is_open() || !f2.is_open())
	{
		std::cout << "Error opening file\n";
		exit(EXIT_FAILURE);
	}

	readHistory(f1, parallelCase);
	readHistory(f2, idealCase);

	f1.close();
	f2.close();

	setOrder(parallelCase, idealCase);
}
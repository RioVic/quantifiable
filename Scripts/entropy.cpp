#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <vector>

struct operation
{
	long key;
	long long timestamp;
}

std::vector<operation> parallelCase;
std::vector<operation> idealCase;

bool compareOperation(operation t1, operation t2)
{
	return (t1.timestamp < t2.timestamp);
}

//Reads the history file into a vector and sorts it by timestamp
void readHistory(ifstream f, std::vector<operation> &v)
{
	string line;
	getline(f, line); //Slip first line

	while (getline(f, line))
	{
		std::stringstream ss(line);
		std::vector<std::string> lineElems;
		while (getline(ss, line, "\t"))
		{
			lineElems.push_back(line);
		}

		struct operation op;
		op.key = lineElems[10];
		op.timestamp = lineElems[9];
		v.push_back(op);
	}

	std::sort(history.begin(), history.end(), compareTimestamp);
}

void compareKeys(std::vector<operation> pCase, std::vector<operation> iCase)
{
	return;
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cout << "Please use: " << argv[0] << " <Entropy Data File 1 (Parallel Case)> <Entropy Data File 2 (Ideal Case)>\n";
	}

	ifstream f1;
	ifstream f2;

	f1.open(argv[1]);
	f2.open(argv[2]);

	if (!f1.is_open() || f2.is_open())
	{
		std::cout << "Error opening file\n";
		exit(EXIT_FAILURE);
	}

	readHistory(f1, parallelCase);
	readHistory(f2, idealCase);

	f1.close();
	f2.close();
}
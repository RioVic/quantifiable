#include <atomic>
#include <cstdio>
#include <ctime>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>

#ifndef OPERATION
#define OPERATION

enum Operation { Push, Pop, Fork };

#endif

template<typename T>
class QStack_NoBranch
{
public:
	class Node;

	QStack_NoBranch(int num_threads, int num_ops) :
		num_threads(num_threads)
	{
		top = new std::atomic<Node*>[num_threads];
		NodeAlloc = new Node*[num_threads];

		for (int i = 0; i < num_threads; i++)
		{
			Node *node = new Node();
			node->sentinel(true);
			top[i] = node;

			threadIndex.push_back(i);

			NodeAlloc[i] = new Node[num_ops];
		}
	}

	~QStack_NoBranch()
	{
		for (int i = 0; i < num_threads; i++)
		{
			delete[] NodeAlloc[i];
		}

		delete[] NodeAlloc;
	}

	bool push(int tid, int opn, T v);

	bool pop(int tid, int opn, T &v);

	void dumpNodes(std::ofstream &p);

	std::vector<int> headIndexStats = {0,0,0,0,0,0,0,0};
	std::vector<int> threadIndex;

private:
	std::atomic<Node *> *top; // node pointer array for branches
	Node **NodeAlloc; //Array for pre-allocated data
	int num_threads;
};

template<typename T>
bool QStack_NoBranch<T>::push(int tid, int opn, T v)
{
	//std::cout << "Push:\n";
	while (true)
	{
		//Read head
		Node *cur = top[tid].load();
		//std::cout << cur << "\n";

		//Regular Case (Perform Add)
		if (cur->isSentinel() || cur->type() == Push)
		{
			//Extract pre-allocated node
			Node *newNode = &NodeAlloc[tid][opn];
			newNode->value(v);
			newNode->type(Push);

			//Set our new node's next pointer to cur
			newNode->next(cur);

			if (top[tid].compare_exchange_weak(cur, newNode))
			{
				return true;
			}
			else 
			{
				continue;
			}
		}
		else //Inverse Stack Case (Perform Remove)
		{
			std::cout << "Push: Inverse Case\n";
			if (top[tid].compare_exchange_weak(cur, cur->next()))
			{
				continue;
			}
			else
			{
				return true;
			}
		}
	}
}

template<typename T>
bool QStack_NoBranch<T>::pop(int tid, int opn, T& v)
{
	//std::cout << "Pop:\n";
	while (true)
	{
		//Read head
		Node *cur = top[tid].load();
		//std::cout << cur << "\n";

		//Inverse Stack Case (Perform Add)
		if (cur->isSentinel() || cur->type() == Pop)
		{
			std::cout << "Pop: Inverse Case\n";
			Node *newNode = &NodeAlloc[tid][opn];
			newNode->type(Pop);
			newNode->next(cur);

			if (top[tid].compare_exchange_weak(cur, newNode))
			{
				return true;
			}
			else 
			{
				continue;
			}
		}
		else // Regular case (Perform Remove)
		{
			if (top[tid].compare_exchange_weak(cur, cur->next()))
			{
				return true;
			}
			else
			{
				continue;
			}
		}
	}
}

template<typename T>
void QStack_NoBranch<T>::dumpNodes(std::ofstream &p)
{
	for (int i = 0; i < this->num_threads; i++)
	{
		Node *n = this->top[i].load();
		Node *pred = nullptr;
		if (n == nullptr)
			continue;

		//Terminate when we reach the main branch, or when the main branch reaches the end
		do
		{
			p << std::left << "Address: " << std::setw(10) << n << "\t\tNext: " << std::setw(10) << n->next() << "\t\tValue: " << std::setw(10) << n->value() << "\t\tType:" << std::setw(10) << (Operation)n->type() << "\n";;
			pred = n;
			n = n->next();
		} while (n != nullptr);
	}
}

template<typename T>
class QStack_NoBranch<T>::Node
{
public:
	Node (T &v) : _val(v), _pred() {};
	Node () : _pred() {};

	T value() { return _val; };
	void value(T &v) { _val = v; };

	void next(Node *n) {_next = n; };
	Node *next() { return _next; };

	void pred(Node *p) { _pred = p; };
	Node **pred() { return _pred; };
	void removePred() { _pred = nullptr; };

	void sentinel(bool b) { _sentinel = b; };
	bool isSentinel() { return _sentinel; };

	void type(Operation type) { _type = type; };
	Operation type() { return _type; };

	void level(int i) { _branch_level = i; };
	int level() { return _branch_level; };

private:
	T _val {NULL};
	Node *_next {nullptr};
	Node *_pred {nullptr};
	int _branch_level;
	bool _sentinel = false;
	Operation _type;
	
};
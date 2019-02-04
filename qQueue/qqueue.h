#include <atomic>
#include <cstdio>
#include <ctime>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>

#define CAS_LIMIT 1
#define MAX_FORK_AT_NODE 3

enum Operation { Enqueue, Dequeue };

template<typename T>
class QQueue
{
public:
	class Node;

	QQueue(int num_threads, int num_ops) :
		num_threads(num_threads)
	{
		head = new std::atomic<Node*>[num_threads];
		tail = new std::atomic<Node*>[num_threads];
		NodeAlloc = new Node*[num_threads];

		for (int i = 0; i < num_threads; i++)
		{
			head[i] = nullptr;
			tail[i] = nullptr;
			threadIndex.push_back(i);

			NodeAlloc[i] = new Node[num_ops];
		}
	}

	~QQueue()
	{
		for (int i = 0; i < num_threads; i++)
		{
			delete[] NodeAlloc[i];
		}

		delete[] NodeAlloc;
	}

	bool enqueue(int tid, int opn, T v);

	bool dequeue(int tid, int opn, T &v);

	std::vector<int> threadIndex;
	int branches = 1;

private:
	std::atomic<Node *> *head; // node pointer array for branches
	std::atomic<Node *> *tail; // node pointer array for branches
	Node **NodeAlloc; //Array for pre-allocated data
	int num_threads;
};

template<typename T>
bool QQueue<T>::enqueue(int tid, int opn, T v)
{
	//Extract pre-allocated node
	Node *elem = &NodeAlloc[tid][opn];
	elem->value(v);
	elem->op(Enqueue);
	int Index = threadIndex[tid];

	while (true)
	{
		//Read front of queue
		Node *cur = head[Index].load();

		//Check if the queue is empty, or that there are no pending dequeue operations that need to be matched
		if (cur != nullptr || cur->op() == Enqueue)
		{
			//Enqueue as normal
			elem->next(cur);
	
			if (head[Index].compare_exchange_weak(cur, elem))
				return true;
		}
		else
		{
			//Remove the pending dequeue operation
			if (head[Index].compare_exchange_weak(cur, cur->next()))
				return true;
		}	
	}
}

template<typename T>
bool QQueue<T>::dequeue(int tid, int opn, T& v)
{
	int Index = threadIndex[tid];

	while (true)
	{
		//Read front of queue
		Node *cur = tail[Index].load();

		//Check if the queue if there are any nodes to dequeue, or that there are other dequeues waiting for a matching enqueue
		if (cur == nullptr || cur->op() == Dequeue)
		{
			//Add this dequeue operation to the queue as a pending operation
			Node *elem = &NodeAlloc[tid][opn];
			elem->value(v);
			elem->op(Dequeue);
			cur = head[Index].load();

			if (head[Index].compare_exchange_weak(cur, elem))
				return true;
		}
		else
		{
			//Dequeue as normal
			if (tail[Index].compare_exchange_weak(cur, cur->next()))
			{
				v = cur->value();
				return true;
			}
		}
	}
}

template<typename T>
class QQueue<T>::Node
{
public:
	Node (T &v) : _val(v) {};
	Node () {};

	T value() { return _val; };
	void value(T &v) { _val = v; };

	void next(Node *n) {_next = n; };
	Node *next() { return _next; };

	Operation op() { return _op; };
	void op(Operation op) { _op = op; };

private:
	T _val {NULL};
	Node *_next {nullptr};
	Operation _op;
};
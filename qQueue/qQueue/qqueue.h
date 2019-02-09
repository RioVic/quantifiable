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
		_head = new std::atomic<Node*>[num_threads];
		_tail = new std::atomic<Node*>[num_threads];
		NodeAlloc = new Node*[num_threads];

		for (int i = 0; i < num_threads; i++)
		{
			_head[i] = nullptr;
			_tail[i] = nullptr;
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
	std::atomic<Node *> *_head; // node pointer array for branches
	std::atomic<Node *> *_tail; // node pointer array for branches
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
	Node *head;
	Node *tail;
	Node *tail_next;
	Node *head_next;

	while (true)
	{
		//Read the queue
		head = _head[Index].load();
		tail = _tail[Index].load();
		tail_next = tail->next.load();
		head_next = head->next.load();

		//Lazily catch up tail pointer
		if (tail_next == nullptr)
		{
			_tail[Index].compare_exchange_weak(tail, tail_next);
			continue;
		}

		//Check if the queue is empty, or that there are no pending dequeue operations that need to be matched
		if (head_next == nullptr || tail->op() == Enqueue)
		{
			//Enqueue as normal
			if (tail->next.compare_exchange_weak(tail_next, elem))
			{
				//Lazily try to update the tail pointer
				_tail[Index].compare_exchange_weak(tail, elem);
				return true;
			}
		}
		else
		{
			//Remove the pending dequeue operation at the head
			if (_head[Index].compare_exchange_weak(head, head_next))
				return true;
		}	
	}
}

template<typename T>
bool QQueue<T>::dequeue(int tid, int opn, T& v)
{
	int Index = threadIndex[tid];
	Node *head;
	Node *tail;
	Node *tail_next;
	Node *head_next;

	while (true)
	{
		//Read the queue
		head = _head[Index].load();
		tail = _tail[Index].load();
		tail_next = tail->next.load();
		head_next = head->next.load();

		//Lazily catch up tail pointer
		if (tail_next == nullptr)
		{
			_tail[Index].compare_exchange_weak(tail, tail_next);
			continue;
		}

		//Check if the queue if there are any nodes to dequeue, or that there are other dequeues waiting for a matching enqueue
		if (head_next == nullptr || tail->op() == Dequeue)
		{
			//Add this dequeue operation to the queue as a pending operation
			Node *elem = &NodeAlloc[tid][opn];
			elem->value(v);
			elem->op(Dequeue);
			
			if (tail->next.compare_exchange_weak(tail_next, elem))
			{
				//Lazily try to update the tail pointer
				_tail[Index].compare_exchange_weak(tail, elem);
				return true;
			}
		}
		else
		{
			v = head->value();
			//Dequeue as normal
			if (_head[Index].compare_exchange_weak(head, head_next))
				return true;
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

	Operation op() { return _op; };
	void op(Operation op) { _op = op; };

	std::atomic<Node *> next {nullptr};

private:
	T _val {NULL};
	Operation _op;
};
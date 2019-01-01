#include <atomic>
#include <cstdio>
#include <ctime>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>

#define CAS_LIMIT 3

enum Operation { Push, Pop, Fork };

template<typename T>
class QStack
{
public:
	class Desc;
	class Node;

	QStack(int num_threads, int num_ops) :
		data(num_threads),
		num_threads(num_threads),
		forkRequest(0)
	{
		top = new std::atomic<Node*>[num_threads];
		for (int i = 0; i < num_threads; i++)
		{
			top[i] = nullptr;
		}

		for (auto &v : data)
		{
			for (int i = 0; i < num_ops / num_threads; i++)
			{
				v.push_back(new Node());
			}
		}
	}

	bool push(int tid, int opn, T v);

	bool pop(int tid, int opn, T &v);

	void dumpNodes(std::ofstream &p);

private:
	std::atomic<Node *> *top; // node pointer array for branches
	std::vector<std::vector<Node *>> data; //Array for pre-allocated data
	int num_threads;
	std::atomic<int> forkRequest;
};

template<typename T>
bool QStack<T>::push(int tid, int opn, T v)
{
	if (data[tid].size() == 0)
		return false;

	//Extract pre-allocated node
	Node *elem = std::move(data[tid].back());
	data[tid].pop_back();
	elem->value(v);
	elem->desc = new Desc(Push);

	int loop = 0;
	int headIndex = 0;

	while (true)
	{
		int req = forkRequest.load();

		if (req)
			elem->desc.load()->op(Fork);

		//Read top of stack
		Node *cur = top[headIndex].load();
		Desc *cur_desc = cur->desc.load();
		elem->next(cur);
		elem->level(headIndex);
		
		//If no operation is occuring at current node
		if (cur_desc->active == false)
		{
			//Place descriptor in node
			if (cur->desc.compare_exchange_weak(cur_desc, elem->desc))
			{
				//Update head (can be done without CAS since we own the current head of this branch via descriptor)
				top[headIndex] = elem;

				//Try to satisfy the fork request
				if (req && forkRequest.compare_exchange_weak(req, 0))
				{
					//Point an available head pointer at curr
					for (int i = 0; i < num_threads; i++)
					{
						int index = (i + headIndex + 1) % this->num_threads;
						Node *top_node = top[index];
						
						if (top_node == nullptr)
						{
							if (top[index].compare_exchange_weak(top_node, cur))
								break;
						}
					}
				}
			}
		}

		// If we see a lot of contention create a fork request
		if ((cur != nullptr) && loop > CAS_LIMIT)
		{
			int req = forkRequest.load();

			if (!req)
			{
				forkRequest.compare_exchange_weak(req, 1);
			}
		}
		++loop;
	}

	//Mark operation done
	elem->desc.load()->active = false;
}

template<typename T>
bool QStack<T>::pop(int tid, int opn, T& v)
{
	int loop = 0;
	int headIndex = 0;

	while (true)
	{
		Node *cur = top[headIndex].load();
		Node *next = nullptr;

		if (cur != nullptr)
		{
			//If the next node is part of our currrent branch, make it the head of this branch
			//Otherwise, we want the head of the current branch to become null
			if (cur->next()->level() == cur->level())
				next = cur->next();
		}
		else
		{
			//If curr does not exist, the main branch is empty. Push to sub-tree
			v = int('?');
			return false;
		}

		//Pop from selected head
		if (top[headIndex].compare_exchange_weak(cur, next))
		{
			v = cur->value();
			//delete cur;
			return true;
		}

		// If we see a lot of contention try another branch
		if (loop > CAS_LIMIT)
		{
			headIndex = (headIndex + 1) % this->num_threads;
		}
		++loop;
	}
}

template<typename T>
void QStack<T>::dumpNodes(std::ofstream &p)
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
			p << "Address: " << n << "\tValue: " << n->value() << "\tNext: " << n->next() << "\n";
			pred = n;
			n = n->next();
		} while (n != nullptr && n->pred() == pred);
	}
}

template<typename T>
class QStack<T>::Node
{
public:
	Node (T &v) : _val(v) {};
	Node () {};

	T value() { return _val; };
	void value(T &v) { _val = v; };

	void next(Node *n) {_next = n; };
	Node *next() { return _next; };

	void pred(Node *p) { _pred = p; };
	Node *pred() { return _pred; };

	void level(int i) { _branch_level = i; };
	int level() { return _branch_level; };

	//void desc(std::atomic<QStack::Desc *> desc) { _desc = desc; };
	//std::atomic<QStack::Desc> desc() { return _desc; };

	std::atomic<QStack::Desc *> desc;

private:
	T _val {NULL};
	Node *_next {nullptr};
	Node *_pred {nullptr};
	int _branch_level;
	
};

template<typename T>
class QStack<T>::Desc
{
public:
	Desc (Operation op) : _op(op) { active = true; };
	Desc () { active = true; };

	Operation op() { return _op; };
	void op(Operation o) { _op = o; };

	std::atomic<bool> active;

private:
	Operation _op;
	
};
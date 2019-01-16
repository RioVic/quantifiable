#include <atomic>
#include <cstdio>
#include <ctime>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>

#define CAS_LIMIT 3
#define MAX_FORK_AT_NODE 3

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

		Node *s = new Node();
		s->sentinel(true);
		top[0] = s;
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
	Desc *d = new Desc(Push);

	int loop = 0;
	int headIndex = 0;

	while (true)
	{
		//Check for fork request
		int req = forkRequest.load();

		//If it exists, change our current op to a fork operation
		if (req)
			d->op(Fork);

		//Read top of stack
		Node *cur = top[headIndex].load();
		Desc *cur_desc = (cur != nullptr ? cur->desc.load() : nullptr);
		elem->next(cur);
		elem->level(headIndex);
		
		//If no operation is occuring at current node
		if (cur_desc == nullptr || cur_desc->active == false)
		{
			//Place our descriptor in the node
			if (cur->desc.compare_exchange_weak(cur_desc, d))
			{
				//Update head (can be done without CAS since we own the current head of this branch via descriptor)
				top[headIndex] = elem;

				//Since this node is at the head, we know it has room for at least 1 more predecessor, so we add our current element
				cur->addPred(elem);

				//Try to satisfy the fork request if it exists
				if (req && forkRequest.compare_exchange_weak(req, 0))
				{
					//Point an available head pointer at curr
					for (int i = 0; i < num_threads; i++)
					{
						int index = (i + headIndex + 1) % this->num_threads;
						Node *top_node = top[index];
						
						if (top_node == nullptr)
						{
							//CAS is necesary here since another thread may be trying to use this slot for a different head node
							if (top[index].compare_exchange_weak(top_node, cur))
								break;
						}
					}
				}
			}
		}

		++loop;

		// If we see a lot of contention create a fork request
		if ((cur != nullptr) && loop > CAS_LIMIT)
		{
			int req = forkRequest.load();

			if (!req)
			{
				forkRequest.compare_exchange_weak(req, 1);
			}

			loop = 0;
		}
	}

	//Mark operation done for other threads
	d->active = false;
}

template<typename T>
bool QStack<T>::pop(int tid, int opn, T& v)
{
	int loop = 0;
	int headIndex = 0;
	Desc *d = new Desc(Pop);

	while (true)
	{
		Node *cur = top[headIndex].load();
		Node *next = nullptr;

		if (!cur->isSentinel())
		{
			Desc *cur_desc = cur->desc.load();

			//Check for pending operations or head pointer updates
			if (cur_desc->active == false && top[headIndex] == cur)
			{
				//Place descriptor in node
				if (cur->desc.compare_exchange_weak(cur_desc, d))
				{
					//Check that the pred array is empty, and that a safe pop can occur
					if (cur->hasNoPreds())
					{
						v = cur->value();
						top[headIndex] = next;
						//delete cur;
						return true;
					}
					else
					{
						//If there is a pred pointer, we cannot pop this node. We must go somewhere else
						headIndex = (headIndex + 1) % this->num_threads;
						loop = 0;
					}
				}
			}
		}
		else
		{
			//If curr does not exist, the main branch is empty. Push to sub-tree
			v = int('?');
			return false;
		}

		++loop;

		// If we see a lot of contention try another branch
		if (loop > CAS_LIMIT)
		{
			headIndex = (headIndex + 1) % this->num_threads;
			loop = 0;
		}
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
		} while (n != nullptr && n->level() == pred->level());
	}
}

template<typename T>
class QStack<T>::Node
{
public:
	Node (T &v) : _val(v), _pred() {};
	Node () : _pred() {};

	T value() { return _val; };
	void value(T &v) { _val = v; };

	void next(Node *n) {_next = n; };
	Node *next() { return _next; };

	void pred(Node *p, int i) { _pred[i] = p; };
	Node **pred() { return _pred; };

	void sentinel(bool b) { _sentinel = b; };
	bool isSentinel() { return _sentinel; };

	void addPred(Node *p) 
	{ 
		for (auto &n : _pred)
		{
			if (n == nullptr)
				n = p;
		}
	}

	void removePred(Node *p)
	{
		for (auto &n : _pred)
		{
			if (n == p)
				n = nullptr;
		}
	}

	bool hasNoPreds()
	{
		for (auto &n : _pred)
		{
			if (n != nullptr)
				return true;
		}

		return false;
	}

	void level(int i) { _branch_level = i; };
	int level() { return _branch_level; };

	//void desc(std::atomic<QStack::Desc *> desc) { _desc = desc; };
	//std::atomic<QStack::Desc> desc() { return _desc; };

	std::atomic<QStack::Desc *> desc;

private:
	T _val {NULL};
	Node *_next {nullptr};
	Node *_pred[MAX_FORK_AT_NODE];
	int _branch_level;
	int _predIndex = 0;
	bool _sentinel = false;
	
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
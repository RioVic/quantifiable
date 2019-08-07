#include "rdtsc.h"
#include <atomic>
#include <cstdio>
#include <stdlib.h>
#include <ctime>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <boost/random.hpp>

#define CAS_LIMIT 1
#define MAX_FORK_AT_NODE 3
#define MAX_DEPTH_DISPARITY 5

#ifndef OP_D
#define OP_D
enum Operation { Push, Pop, Fork };
#endif

template<typename T>
class QStackDesc
{
public:
	class Desc;
	class Node;

	QStackDesc(int num_threads, int num_ops) :
		num_threads(num_threads),
		forkRequest(0)
	{
		top = new std::atomic<Node*>[num_threads];
		topDepths = new std::atomic<int>[num_threads];
		NodeAlloc = new Node*[num_threads];
		DescAlloc = new Desc*[num_threads];
		randomGen = new boost::mt19937[num_threads];
		randomDist = new boost::uniform_int<uint32_t>[num_threads];

		for (int i = 0; i < num_threads; i++)
		{
			top[i] = nullptr;
			topDepths[i] = -1;
			threadIndex.push_back(0);

			NodeAlloc[i] = new Node[num_ops*num_threads*2];
			DescAlloc[i] = new Desc[num_ops*num_threads*2];
			randomGen[i].seed(time(0));
			randomDist[i] = boost::uniform_int<uint32_t>(0, num_threads-1);
		}

		topDepths[0] = 0;
		Node *s = new Node();
		s->sentinel(true);
		top[0] = s;
	}

	~QStackDesc()
	{
		for (int i = 0; i < num_threads; i++)
		{
			delete[] NodeAlloc[i];
			delete[] DescAlloc[i];
		}

		delete[] NodeAlloc;
		delete[] DescAlloc;
	}

	bool push(int tid, int opn, T ins, T &v, int &popOpn, int &popThread);

	bool pop(int tid, int opn, T &v);

	bool add(int tid, int opn, int index, Node *cur, Node *elem);

	bool remove(int tid, int opn, T &v, int index, Node *cur, int &popOpn, int &popThread);

	void dumpNodes(std::ofstream &p);

	bool isEmpty();

	std::vector<int> threadIndex;
	int branches = 1;
	boost::uniform_int<uint32_t> *randomDist;
	boost::mt19937 *randomGen;

private:
	std::atomic<Node *> *top; // node pointer array for branches
	std::atomic<int> *topDepths;
	Node **NodeAlloc; //Array for pre-allocated data
	Desc **DescAlloc; //Array for pre-allocated data
	int num_threads;
	std::atomic<int> forkRequest;
};

template<typename T>
bool QStackDesc<T>::push(int tid, int opn, T ins, T &v, int &popOpn, int &popThread)
{
	//Extract pre-allocated node
	Node *elem = &NodeAlloc[tid][opn];
	elem->value(ins);
	elem->type(Push);
	Desc *d = &DescAlloc[tid][opn];

	int loop = 0;
	threadIndex[tid] = tid;

	while (true)
	{
		//Thread preferred push (Take the help index if another branch is falling behind)
		int headIndex = threadIndex[tid];

		//Read top of stack
		Node *cur = top[headIndex].load();

		if (cur == nullptr) 
		{
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			continue;
		}

		int preferred = cur->depth();
		//Check for lagging branch
		for (int i = 0; i < num_threads; i++)
		{
			Node *n = top[i].load();

			if (n == nullptr || i == headIndex)
				continue;

			int d = n->depth();
			if (preferred - d >= MAX_DEPTH_DISPARITY)
			{
				//std::cout << preferred << " > " << d << " switching from " << headIndex << " to " << i << "\n";
				cur = n;
				break;
			}
		}

		Desc *cur_desc = cur->desc.load();
		elem->next(cur);
		
		//If no operation is occuring at current node
		if ((cur_desc == nullptr || cur_desc->active == false) && top[headIndex] == cur)
		{
			//Place our descriptor in the node
			if (cur->desc.compare_exchange_weak(cur_desc, d))
			{
				//Check for ABA?
				if (top[headIndex] != cur)
				{
					cur->desc = nullptr;
					continue;
				}

				if (cur->isSentinel() || cur->type() == Push)
				{
					//Add node as usual
					this->add(tid, opn, headIndex, cur, elem);

					d->active = false;
					return true;
				}
				else
				{
					std::cout << "Inverse Stack Disabled\n";
					exit(EXIT_FAILURE);

					//Remove this node instead of pushing
					if (!this->remove(tid, opn, v, headIndex, cur, popOpn, popThread))
					{
						//Remove desc and retry
						cur->desc = nullptr;
					}
					else
					{
						d->active = false;
						return true;
					}
				}
			}
		}

		loop++;

		// If we see a lot of contention create a fork request
		if (loop > CAS_LIMIT)
		{
			int req = forkRequest.load();

			if (!req && this->branches < this->num_threads)
			{
				forkRequest.compare_exchange_weak(req, 1);
			}

			loop = 0;
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
		}
	}
}

template<typename T>
bool QStackDesc<T>::pop(int tid, int opn, T& v)
{
	int loop = 0;
	Desc *d = &DescAlloc[tid][opn];

	while (true)
	{
		//Random pop
		//int headIndex = randomDist[tid](randomGen[tid]); //Choose pop index randomly
		//End of random pop

		//Depth based pop
		int headIndex = 0;

		int highest = 0;
		int lowest = INT_MAX;
		for (int i = 0; i < num_threads; i++)
		{
			Node *n = top[i].load();

			if (n == nullptr)
				continue;

			int d = n->depth();

			if (d > highest)
			{
				headIndex = i;
				highest = d;
			}

			if (d < lowest)
			{
				lowest = d;
			}
		}

		if (highest - lowest > MAX_DEPTH_DISPARITY)
			std::cout << "Disparity of " << highest - lowest << " found \n";
		//End of Depth based pop
		
		Node *cur = top[headIndex].load();
		//d->active = true;

		if (cur == nullptr)
		{
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			continue;
		}

		Desc *cur_desc = cur->desc.load();

		//Check for pending operations or head pointer updates
		if ((cur_desc == nullptr || cur_desc->active == false) && top[headIndex] == cur)
		{
			//Place descriptor in node
			if (cur->desc.compare_exchange_weak(cur_desc, d))
			{
				if (top[headIndex] != cur)
				{
					cur->desc = nullptr;
					continue;
				}

				if (cur->isSentinel() || cur->type() == Pop)
				{
					std::cout << "Inverse Stack Disabled\n";
					exit(EXIT_FAILURE);

					Node *elem = &NodeAlloc[tid][opn];
					elem->type(Pop);
					elem->next(cur);

					//Append pop operation instead of removing (there are no available nodes to pop)
					this->add(tid, opn, headIndex, cur, elem);

					d->active = false;
					return true;
				}
				else
				{
					int pushOpn;
					int popThread;

					//Attempt to remove as normal
					if (!this->remove(tid, opn, v, headIndex, cur, pushOpn, popThread))
					{
						cur->desc = nullptr;
						threadIndex[tid] = (headIndex + 1) % this->num_threads;
					}
					else
					{
						d->active = false;
						return true;
					}
				}
			}
		}

		loop++;

		// If we see a lot of contention (And we are in the pop stack) create a fork request
		if (loop > CAS_LIMIT)
		{
			if (cur->type() == Pop)
			{
				int req = forkRequest.load();

				if (!req && this->branches < this->num_threads)
				{
					forkRequest.compare_exchange_weak(req, 1);
				}
			}
			loop = 0;
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
		}
	}
}

//Adds an arbitrary operation to the stack
template<typename T>
bool QStackDesc<T>::add(int tid, int opn, int headIndex, Node *cur, Node *elem)
{
	//Since this node is at the head, we know it has room for at least 1 more predecessor, so we add our current element
	//We must add cur to the pred list before updating top, otherwise this node will be visible without being fully linked
	cur->addPred(elem);

	//Update the depth field for this node to help with balancing the tree
	elem->depth(elem->next()->depth() + 1);
	//topDepths[headIndex] = elem->depth();

	//Update head (can be done without CAS since we own the current head of this branch via descriptor)
	top[headIndex] = elem;

	int req = forkRequest.load();

	//Try to satisfy the fork request if it exists
	if (req && cur->predNotFull(top, num_threads) && forkRequest.compare_exchange_weak(req, 0))
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
				{
					this->branches++;
					break;
				}
			}
		}
	}
}

//Removes an arbitrary operation from the stack
template<typename T>
bool QStackDesc<T>::remove(int tid, int opn, T &v, int headIndex, Node *cur, int &popOpn, int &popThread)
{
	//Check that the pred array is empty, and that a safe pop can occur
	if (cur->hasNoPreds() && cur->notInTop(top, num_threads, headIndex))
	{
		v = cur->value();
		cur->next()->removePred(cur);
		//topDepths[headIndex] = cur->depth();
		top[headIndex] = cur->next();
		return true;
	}
	else
	{
		//If there is a pred pointer, we cannot pop this node. We must go somewhere else
		if (top[headIndex] == cur)
		{
			this->branches--;
			//topDepths[headIndex] = -1;
			top[headIndex] = nullptr;
		}
		return false;
	}	
}

template<typename T>
bool QStackDesc<T>::isEmpty()
{
	for (int i = 0; i < num_threads; i++)
	{
		Node *n = top[i].load();
		if (n != NULL && (n->isSentinel() || n->type() == Pop))
			return true;
	}

	return false;
}

template<typename T>
void QStackDesc<T>::dumpNodes(std::ofstream &p)
{
	for (int i = 0; i < this->num_threads; i++)
	{
		p << "Printing branch: " << i << "\n";
		Node *n = this->top[i].load();
		Node *pred = nullptr;
		if (n == nullptr)
			continue;

		//Terminate when we reach the main branch, or when the main branch reaches the end
		do
		{
			if (n->pred()[1] != nullptr)
			{
				p << "Fork here, " << n->pred()[0] << "\t" << n->pred()[1] << "\n";
			}
			p << std::left << "Address: " << std::setw(10) << n << "\t\tNext: " << std::setw(10) << n->next() << "\t\tValue: " << std::setw(10) << n->value() << "\t\tType:" << std::setw(10) << (Operation)n->type() << std::setw(10) << "\t\tlevel:" << std::setw(10) << n->level() << "\n";
			pred = n;
			n = n->next();
		} while (n != nullptr && n->level() == i);
	}

	p << "branches: " << this->branches << "\n";
}

template<typename T>
class QStackDesc<T>::Node
{
public:
	Node (T &v) : _val(v), _pred(), desc(nullptr) {};
	Node () : _pred(), desc(nullptr) {};

	T value() { return _val; };
	void value(T v) { _val = v; };

	void next(Node *n) {_next = n; };
	Node *next() { return _next; };

	void pred(Node *p, int i) { _pred[i] = p; };
	Node **pred() { return _pred; };

	void sentinel(bool b) { _sentinel = b; };
	bool isSentinel() { return _sentinel; };

	void type(Operation type) { _type = type; };
	Operation type() { return _type; };

	void opn(int n) { _opn = n; };
	int opn() { return _opn; };

	void thread(int tid) { _thread = tid; };
	int thread() { return _thread; };

	int depth(int d) { _depth = d; };
	int depth() { return _depth; };

	bool predNotFull(std::atomic<Node *> *top, int num_threads)
	{
		int pointerCount = 0;
		for (int i = 0; i < num_threads; i++)
		{
			Node *n = top[i];
			if (n == this)
				pointerCount++;
		}

		for (auto &n : _pred)
		{
			if (n == nullptr && pointerCount-- == 0)
			{
				return true;
			}
		}

		return false;
	}

	void addPred(Node *p) 
	{ 
		for (auto &n : _pred)
		{
			if (n == nullptr)
			{
				n = p;
				return;
			}
		}
	}

	bool verifyLink(std::atomic<Node *> *top)
	{
		if (_next == nullptr)
		{
			if (isSentinel())
				return true;

			for (int i = 0; i < 4; i++)
			{
				if (top[i] == this)
					return false;
			}
			return true;
		}

		Node **preds = _next->pred();
		for (int i = 0; i < 2; i++)
		{
			Node *n = preds[i];
			if (n == this)
			{
				return true;
			}
		}

		return false;
	}

	void removePred(Node *p)
	{
		for (int i = 0; i < MAX_FORK_AT_NODE; i++)
		{
			Node *n = _pred[i];
			if (n == p)
			{
				_pred[i] = nullptr;
				return;
			}
		}
	}

	bool hasNoPreds()
	{
		for (int i = 0; i < MAX_FORK_AT_NODE; i++)
		{
			Node *n = _pred[i];
			if (n != nullptr)
				return false;
		}

		return true;
	}

	bool notInTop(std::atomic<Node *> *top, int num_threads, int safeIndex)
	{
		for (int i = 0; i < num_threads; i++)
		{
			Node *n = top[i];
			if (i != safeIndex && n == this)
				return false;
		}

		return true;
	}

	void level(int i) { _branch_level = i; };
	int level() { return _branch_level; };

	//void desc(std::atomic<QStackDesc::Desc *> desc) { _desc = desc; };
	//std::atomic<QStackDesc::Desc> desc() { return _desc; };

	std::atomic<QStackDesc::Desc *> desc;

private:
	T _val {NULL};
	Node *_next {nullptr};
	Node *_pred[MAX_FORK_AT_NODE];
	int _branch_level;
	int _predIndex = 0;
	bool _sentinel = false;
	Operation _type;
	int _opn;
	int _thread;
	int _depth = 0;
	
};

template<typename T>
class __attribute__((aligned(64))) QStackDesc<T>::Desc
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
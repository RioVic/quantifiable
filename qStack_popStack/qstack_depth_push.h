#include <atomic>
#include <cstdio>
#include <ctime>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>

#define CAS_LIMIT 1
#define MAX_FORK_AT_NODE 3
#define MAX_DEPTH_DISPARITY 5

enum Operation { Push, Pop, Fork };

template<typename T>
class QStack
{
public:
	class Desc;
	class Node;

	QStack(int num_threads, int num_ops) :
		num_threads(num_threads),
		forkRequest(0)
	{
		top = new std::atomic<Node*>[num_threads];
		NodeAlloc = new Node*[num_threads];
		DescAlloc = new Desc*[num_threads];

		for (int i = 0; i < num_threads; i++)
		{
			top[i] = nullptr;
			threadIndex.push_back(0);
			topDepth[i*8] = 0;

			std::cout << threadIndex[i] << "\n";

			NodeAlloc[i] = new Node[num_ops];
			DescAlloc[i] = new Desc[num_ops];
		}

		Node *s = new Node();
		s->sentinel(true);
		top[0] = s;
	}

	~QStack()
	{
		for (int i = 0; i < num_threads; i++)
		{
			delete[] NodeAlloc[i];
			delete[] DescAlloc[i];
		}

		delete[] NodeAlloc;
		delete[] DescAlloc;
	}

	bool push(int tid, int opn, T v);

	bool pop(int tid, int opn, T &v);

	bool add(int tid, int opn, T v, int index, Node *cur, Node *elem);

	bool remove(int tid, int opn, T &v, int index, Node *cur);

	void dumpNodes(std::ofstream &p);

	uint64_t topDepth[64 * 8];
	std::vector<int> threadIndex;
	int branches = 1;

private:
	std::atomic<Node *> *top; // node pointer array for branches
	Node **NodeAlloc; //Array for pre-allocated data
	Desc **DescAlloc; //Array for pre-allocated data
	int num_threads;
	std::atomic<int> forkRequest;
};

template<typename T>
bool QStack<T>::push(int tid, int opn, T v)
{
	//Extract pre-allocated node
	Node *elem = &NodeAlloc[tid][opn];
	elem->value(v);
	elem->type(Push);
	Desc *d = &DescAlloc[tid][opn];

	int loop = 0;

	while (true)
	{
		//Check for fork request
		int headIndex = threadIndex[tid];
		int req = forkRequest.load();

		//Read top of stack
		Node *cur = top[headIndex].load();

		if (cur == nullptr) 
		{
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			continue;
		}

		int preferred = topDepth[headIndex*8];
		//Check for lagging branch
		for (int i = (headIndex + 1) % num_threads; i < (headIndex + 3) % num_threads; i++)
		{
			if (i == headIndex)
				continue;

			int d = topDepth[i*8];

			if (d == 0)
				continue;

			if (preferred - d >= MAX_DEPTH_DISPARITY)
			{
				//std::cout << preferred << " > " << d << " switching from " << headIndex << " to " << i << "\n";
				threadIndex[tid] = i;
				break;
			}
		}

		Desc *cur_desc = cur->desc.load();
		elem->next(cur);
		
		//If no operation is occuring at current node
		if (cur_desc == nullptr || cur_desc->active == false)
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
					this->add(tid, opn, v, headIndex, cur, elem);
					d->active = false;
					return true;
				}
				else
				{
					//Remove this node instead of pushing
					if (!this->remove(tid, opn, v, headIndex, cur))
					{
						//Remove desc and retry
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

		++loop;

		// If we see a lot of contention create a fork request
		if ((cur != nullptr) && loop > CAS_LIMIT)
		{
			int req = forkRequest.load();

			if (!req && branches < this->num_threads)
			{
				forkRequest.compare_exchange_weak(req, 1);
			}

			loop = 0;
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			//headIndex = (headIndex + 1) % this->num_threads;
		}
	}

	//Mark operation done for other threads
	d->active = false;
}

template<typename T>
bool QStack<T>::pop(int tid, int opn, T& v)
{
	int loop = 0;
	Desc *d = &DescAlloc[tid][opn];

	while (true)
	{
		int headIndex = threadIndex[tid];
		Node *cur = top[headIndex].load();

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
				//Check for ABA?
				if (top[headIndex] != cur)
				{
					cur->desc = nullptr;
					continue;
				}

				if (cur->isSentinel() || cur->type() == Pop)
				{
					Node *elem = &NodeAlloc[tid][opn];
					elem->type(Pop);
					elem->next(cur);

					//Append pop operation instead of removing (there are no available nodes to pop)
					this->add(tid, opn, v, headIndex, cur, elem);
					d->active = false;
					return true;
				}
				else
				{
					//Attempt to remove as normal
					if (!this->remove(tid, opn, v, headIndex, cur))
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

		++loop;

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
bool QStack<T>::add(int tid, int opn, T v, int headIndex, Node *cur, Node *elem)
{
	//Since this node is at the head, we know it has room for at least 1 more predecessor, so we add our current element
	cur->addPred(elem);

	elem->depth(elem->next()->depth() + 1);
	topDepth[headIndex*8] = elem->depth();

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
					branches++;
					break;
				}
			}
		}
	}
}

//Removes an arbitrary operation from the stack
template<typename T>
bool QStack<T>::remove(int tid, int opn, T &v, int headIndex, Node *cur)
{
	//Check that the pred array is empty, and that a safe pop can occur
	if (cur->hasNoPreds() && cur->notInTop(top, num_threads, headIndex))
	{
		v = cur->value();
		cur->next()->removePred(cur);
		top[headIndex] = cur->next();
		return true;
	}
	else
	{
		//If there is a pred pointer, we cannot pop this node. We must go somewhere else
		if (top[headIndex] == cur)
		{
			this->branches--;
			top[headIndex] = nullptr;
		}
		return false;
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
			p << std::left << "Address: " << std::setw(10) << n << "\t\tNext: " << std::setw(10) << n->next() << "\t\tValue: " << std::setw(10) << n->value() << "\t\tType:" << std::setw(10) << (Operation)n->type() << "\n";;
			pred = n;
			n = n->next();
		} while (n != nullptr);
	}
}

template<typename T>
class QStack<T>::Node
{
public:
	Node (T &v) : _val(v), _pred(), desc(nullptr) {};
	Node () : _pred(), desc(nullptr) {};

	T value() { return _val; };
	void value(T &v) { _val = v; };

	void next(Node *n) {_next = n; };
	Node *next() { return _next; };

	void pred(Node *p, int i) { _pred[i] = p; };
	Node **pred() { return _pred; };

	void sentinel(bool b) { _sentinel = b; };
	bool isSentinel() { return _sentinel; };

	void type(Operation type) { _type = type; };
	Operation type() { return _type; };

	int depth(int d) { _depth = d; };
	int depth() { return _depth; };


	void addPred(Node *p) 
	{ 
		for (auto n : _pred)
		{
			if (n == nullptr)
			{
				n = p;
				return;
			}
		}
	}

	void removePred(Node *p)
	{
		for (auto n : _pred)
		{
			if (n == p)
			{
				n = nullptr;
				return;
			}
		}
	}

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

	bool hasNoPreds()
	{
		for (auto &n : _pred)
		{
			if (n != nullptr)
				return false;
		}

		return true;
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
	Operation _type;
	int _depth = 0;
	
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
#include <atomic>
#include <cstdio>
#include <ctime>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>

#define CAS_LIMIT 3
#define MAX_FORK_AT_NODE 2

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
		NodeAlloc = new Node*[num_threads];
		DescAlloc = new Desc*[num_threads];
		randomGen = new boost::mt19937[num_threads];
		randomDist = new boost::uniform_int<uint32_t>[num_threads];
		forks = new std::atomic<Node*>[num_threads];

		for (int i = 0; i < num_threads; i++)
		{
			top[i] = nullptr;
			threadIndex.push_back(0);

			forks[i] == nullptr;
			NodeAlloc[i] = new Node[num_ops*num_threads*2];
			DescAlloc[i] = new Desc[num_ops*num_threads*2];
			randomGen[i].seed(time(0));
			randomDist[i] = boost::uniform_int<uint32_t>(0, num_threads-1);
		}

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

	bool add(int tid, int opn, T v, int index, Node *cur, Node *elem);

	bool remove(int tid, int opn, T &v, int index, Node *cur, int &popOpn, int &popThread);

	void dumpNodes(std::ofstream &p);

	bool isEmpty();

	std::vector<int> headIndexStats = {0,0,0,0,0,0,0,0};
	std::vector<int> threadIndex;
	int branches = 1;
	boost::uniform_int<uint32_t> *randomDist;
	boost::mt19937 *randomGen;

private:
	std::atomic<Node *> *top; // node pointer array for branches
	Node **NodeAlloc; //Array for pre-allocated data
	Desc **DescAlloc; //Array for pre-allocated data
	int num_threads;
	std::atomic<int> forkRequest;
	std::atomic<Node *> *forks;

};

template<typename T>
bool QStackDesc<T>::push(int tid, int opn, T ins, T &v, int &popOpn, int &popThread)
{
	//Extract pre-allocated node
	Node *elem = &NodeAlloc[tid][opn];
	elem->value(ins);
	elem->type(Push);
	Desc *d = &DescAlloc[tid][opn];
	d->op(Push);

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

		Desc *cur_desc = cur->desc.load();
		elem->next(cur);
		
		//If no operation is occuring at current node
		if ((cur_desc == nullptr || cur_desc->active == false) && top[headIndex] == cur)
		{
			//Place our descriptor in the node
			if (cur->desc.compare_exchange_weak(cur_desc, d))
			{
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
					if (!this->remove(tid, opn, v, headIndex, cur, popOpn, popThread))
					{
						cur->desc = nullptr;
						threadIndex[tid] = (headIndex + 1) % this->num_threads;
						continue;
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

			if (!req && this->branches < this->num_threads)
			{
				forkRequest.compare_exchange_weak(req, 1);
			}

			loop = 0;
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			//headIndex = (headIndex + 1) % this->num_threads;
		}
	}
}

template<typename T>
bool QStackDesc<T>::pop(int tid, int opn, T& v)
{
	int loop = 0;
	Desc *d = &DescAlloc[tid][opn];
	d->op(Pop);

	while (true)
	{
		int headIndex = randomDist[tid](randomGen[tid]); //Choose pop index randomly
		Node *cur = top[headIndex].load();
		d->active = true;

		if (cur == nullptr) 
		{
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			continue;
		}

		Desc *cur_desc = cur->desc.load();

		Node *next = cur->next();
		Desc *next_desc = nullptr;

		//If next is null, we are at the sentinel node, which means that we do not need to grab the next_desc
		if (next != nullptr)
			next_desc = next->desc.load();

		//Check for pending operations or head pointer updates
		if ((cur_desc == nullptr || cur_desc->active == false) && (next_desc == nullptr || next_desc->active == false) && top[headIndex] == cur)
		{
			//Place descriptor in node
			if (cur->desc.compare_exchange_weak(cur_desc, d))
			{
				if (cur->isSentinel() || next->desc.compare_exchange_weak(next_desc, d))
				{
					if (top[headIndex] != cur)
					{
						cur->desc = nullptr;
						if (next != nullptr)
							next->desc = nullptr;
						continue;
					}

					if (cur->isSentinel() || cur->type() == Pop)
					{
						Node *elem = &NodeAlloc[tid][opn];
						elem->type(Pop);
						elem->next(cur);

						//Identifies which method this pop came from for entropy tests
						elem->opn(opn);
						elem->thread(tid);

						//Append pop operation instead of removing (there are no available nodes to pop)
						this->add(tid, opn, v, headIndex, cur, elem);

						d->active = false;
						if (next_desc != nullptr)
							next_desc->active = false;
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
							if (next != nullptr)
								next->desc = nullptr;
							threadIndex[tid] = (headIndex + 1) % this->num_threads;
						}
						else
						{
							d->active = false;
							if (next_desc != nullptr)
								next_desc->active = false;
							return true;
						}
					}
				}
				else
				{
					cur->desc = nullptr;
				}
			}
		}

		++loop;

		// If we see a lot of contention try another branch
		if (loop > CAS_LIMIT)
		{
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			//headIndex = (headIndex + 1) % this->num_threads;
			loop = 0;
		}
	}
}

//Adds an arbitrary operation to the stack
template<typename T>
bool QStackDesc<T>::add(int tid, int opn, T v, int headIndex, Node *cur, Node *elem)
{
	//Since this node is at the head, we know it has room for at least 1 more predecessor, so we add our current element
	//We must add cur to the pred first, as the next line will make elem visible to other threads. This create a window where 
	//cur is in the list but it is not linked to elem
	cur->addPred(elem);

	//Update head (can be done without CAS since we own the current head of this branch via descriptor)
	top[headIndex] = elem;
	elem->level(headIndex);

	int req = forkRequest.load();

	//Try to satisfy the fork request if it exists
	if (cur->predNotFull(top, num_threads) && req && forkRequest.compare_exchange_weak(req, 0))
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
		popOpn = cur->opn();
		popThread = cur->thread();
		cur->next()->removePred(cur);
		top[headIndex] = cur->next();
		cur->next(nullptr);
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

		Node *copy[2];
		Node **preds = _next->pred();
		for (int i = 0; i < 2; i++)
		{
			Node *n = preds[i];
			copy[i] = n;
			if (n == this)
			{
				return true;
			}
		}

		return false;
	}

	void removePred(Node *p)
	{
		for (int i = 0; i < 2; i++)
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
		for (auto &n : _pred)
		{
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
	
};

template<typename T>
class QStackDesc<T>::Desc
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
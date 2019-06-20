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

//Must use CAS to save the marked value back into variable
#define SET_MARK(_p)    ((Node *)(((uintptr_t)(_p)) | 1))
#define CLR_MARK(_p)    ((Node *)(((uintptr_t)(_p)) & ~1))
#define IS_MARKED(_p)     (((uintptr_t)(_p)) & 1)

#ifndef OP_D
#define OP_D
enum Operation { Push, Pop, Fork };
#endif

template<typename T>
class QStack
{
public:
	class Node;

	QStack(int num_threads, int num_ops) :
		num_threads(num_threads),
		forkRequest(0)
	{
		top = new std::atomic<Node*>[num_threads];
		NodeAlloc = new Node*[num_threads];

		for (int i = 0; i < num_threads; i++)
		{
			top[i] = nullptr;
			threadIndex.push_back(0);

			NodeAlloc[i] = new Node[num_ops];
			
			for (int j = 0; j < num_ops; j++)
			{
				NodeAlloc[i][j].value( i + (num_threads * j) ); //Give each thread a counting number to insert
			}
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
		}

		delete[] NodeAlloc;
	}

	bool push(int tid, int opn, T ins, T &v, int &popOpn);

	bool pop(int tid, int opn, T &v);

	bool add(int tid, int opn, T v, int index, Node *cur, Node *elem);

	bool remove(int tid, int opn, T &v, int index, Node *cur, int &popOpn);

	void dumpNodes(std::ofstream &p);

	bool isEmpty();

	std::vector<int> headIndexStats = {0,0,0,0,0,0,0,0};
	std::vector<int> threadIndex;
	int branches = 1;

private:
	std::atomic<Node *> *top; // node pointer array for branches
	Node **NodeAlloc; //Array for pre-allocated data
	int num_threads;
	std::atomic<int> forkRequest;
};

template<typename T>
bool QStack<T>::push(int tid, int opn, T ins, T &v, int &popOpn)
{
	//Extract pre-allocated node
	Node *elem = &NodeAlloc[tid][opn];
	//elem->value(ins); Val already set
	elem->type(Push);
	elem->opn(opn);

	int loop = 0;

	while (true)
	{
		//Get preferred index
		int headIndex = threadIndex[tid];

		//Read top of stack
		Node *cur = top[headIndex].load();
		if (cur == nullptr) 
		{
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			continue;
		}

		elem->next(cur);
		
		if (cur->isSentinel() || cur->type() == Push)
		{
			//Add node as usual
			if (this->add(tid, opn, ins, headIndex, cur, elem))
				return true;
		}
		else
		{
			//Remove this node instead of pushing
			if (!this->remove(tid, opn, v, headIndex, cur, popOpn))
			{
				threadIndex[tid] = (headIndex + 1) % this->num_threads;
			}
			else
			{
				return true;
			}
		}

		++loop;

		// If we see a lot of contention try a different leaf node
		if (loop > CAS_LIMIT)
		{
			loop = 0;
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
		}
	}
}

template<typename T>
bool QStack<T>::pop(int tid, int opn, T& v)
{
	int loop = 0;

	while (true)
	{
		//Get preferred index
		int headIndex = threadIndex[tid];

		//Read top of stack
		Node *cur = top[headIndex].load();
		if (cur == nullptr) 
		{
			threadIndex[tid] = (headIndex + 1) % this->num_threads;
			continue;
		}

		if (cur->isSentinel() || cur->type() == Pop)
		{
			Node *elem = &NodeAlloc[tid][opn];
			elem->type(Pop);
			elem->next(cur);
			elem->opn(opn);

			//Append pop operation instead of removing (there are no available nodes to pop)
			if (this->add(tid, opn, v, headIndex, cur, elem))
				return true;
		}
		else
		{
			int pushOpn;

			//Attempt to remove as normal
			if (!this->remove(tid, opn, v, headIndex, cur, pushOpn))
			{
				threadIndex[tid] = (headIndex + 1) % this->num_threads;
			}
			else
			{
				return true;
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
//Attempts to update the pred pointer of cur to become elem
//If the first pred pointer is taken, use the second
template<typename T>
bool QStack<T>::add(int tid, int opn, T v, int headIndex, Node *cur, Node *elem)
{
	int predIndex = 0;
	int forksAllowed = (branches < MAX_FORK_AT_NODE) ? MAX_FORK_AT_NODE : 1;

	while (predIndex < forksAllowed)
	{
		Node *pred = cur->_pred[predIndex];

		if (pred != nullptr)
		{
			predIndex++;
			continue;
		}

		//Make sure pred pointer is not marked by a concurrent Pop operation
		if (IS_MARKED(pred))
			return false;

		//Link this node to the stack
		if (cur->_pred[predIndex].compare_exchange_weak(pred, elem))
		{
			//Update head: Two cases:

			//Case 1: we inserted along the current branch
			//Update without CAS, as no other threads will be attempting to update this top pointer, since they would first have to performed a CAS
			//at cur->_pred[0]. This thread already did that, so it is the only one that can attempt this update
			if (predIndex == 0)
			{
				top[headIndex] = elem;
				elem->level(headIndex);
				return true;
			}
			else if (predIndex == 1) //Case 2: Initializing a new top pointer. This needs CAS as multiple thread may be trying to do the same at different nodes
			{
				//Re-use branch
				//We got to this fork from a branch that has been completely popped away, but not removed yet
				//If we want, we can re-use it.
				/*if (top[headIndex] == curr)
				{
					top[headIndex] = elem;
					elem->level(headIndex);
				}*/

				//Point an available top pointer at curr
				for (int i = 0; i < num_threads; i++)
				{
					int index = (i + headIndex + 1) % this->num_threads;
					Node *top_node = top[index];
					
					if (top_node == nullptr)
					{
						//CAS is necesary here since another thread may be trying to use this slot for a different head node
						if (top[index].compare_exchange_weak(top_node, elem))
						{
							branches++;
							threadIndex[tid] = index;
							elem->level(index);
							return true;
						}
					}
				}

				//Failed to find an available top pointer, undo our work
				//This can happen if multiple threads all try to branch at the same time
				std::cout << "Warning: Branch limit reached in add\n";
				cur->_pred[predIndex] = nullptr;
			}
		}
		else
		{
			predIndex++;
			continue;
		}

	}

	return false;
}

//Removes an arbitrary operation from the stack
//Check each pred pointer is null, then mark it
//If successful, remove the node
template<typename T>
bool QStack<T>::remove(int tid, int opn, T &v, int headIndex, Node *cur, int &popOpn)
{
	//Concurrent pop is already marking this node, we may not do the same
	if (IS_MARKED(cur->_pred[0].load()))
		return false;

	//Mark the pred nodes 
	for (int i = 0; i < MAX_FORK_AT_NODE; i++)
	{
		Node *pred = cur->_pred[i];

		//Mark pred
		if (pred != nullptr || !cur->_pred[i].compare_exchange_weak(pred, SET_MARK(pred)))
		{
			//In the case that we fail to mark a pointer, we must clean up all other marks we made
			int k = i - 1;
			while (k >= 0)
			{
				Node *p = cur->_pred[k];
				//Note: this should never fail?
				cur->_pred[k].compare_exchange_weak(p, CLR_MARK(p));
				k--;
			}

			return false;
		}
	}
	
	//Marking complete, we are free to remove the node from the stack
	v = cur->value();
	popOpn = cur->opn();

	//We need CAS for the unlinking cur, since a pop operation may be attempting to pop cur->next() (Even though it would be unable to)
	Node *next = cur->next();
	int i;
	for (i = 0; i < MAX_FORK_AT_NODE; i++)
	{
		if (next->_pred[i] == cur)
		{
			if (next->_pred[i].compare_exchange_weak(cur, nullptr))
				break;
			else
			{
				std::cout << "Error, CAS on _pred should not fail here\n";
				exit(0);
			}
		}
	}

	//We have popped our way back to a fork node, so the current top pointer should be nulled
	if (i > 0)
	{
		top[headIndex] = nullptr;
	}
	else
	{
		//Since this thread did the marking, no other thread may update top[headIndex] (Push and Pop would stop after seeing the marked pointer)
		top[headIndex] = cur->next();
	}

	return true;
}

template<typename T>
bool QStack<T>::isEmpty()
{
	for (int i = 0; i < num_threads; i++)
	{
		Node *n = top[i].load();
		if (n != NULL && n->isSentinel())
			return true;
	}

	return false;
}

template<typename T>
void QStack<T>::dumpNodes(std::ofstream &p)
{
	for (int i = 0; i < this->num_threads; i++)
	{
		p << "\nPrinting branch: " << i << "\n";
		Node *n = this->top[i].load();
		Node *pred = nullptr;
		if (n == nullptr)
			continue;

		//Terminate when we reach the main branch, or when the main branch reaches the end
		do
		{
			p << std::left << "Address: " << std::setw(10) << n << std::setw(10) << "\t\tPrev[0]: " << std::setw(10) 
			<< n->_pred[0] << std::setw(10) << "\t\tPrev[1]: " << std::setw(10) << n->_pred[1] << std::setw(10) << "\t\tNext: " 
			<< n->next() << "\t\tValue: " << std::setw(10) << n->value() << "\t\tType:" << std::setw(10) << (Operation)n->type() 
			<< "\t\tlevel:" << std::setw(10) << n->level() << "\n";

			pred = n;
			n = n->next();
		} while (n != nullptr && n->level() == i);
	}
}

template<typename T>
class QStack<T>::Node
{
public:
	Node (T &v) : _val(v), _pred() {};
	Node () : _pred() {};

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

	void removePred(Node *p)
	{
		for (auto &n : _pred)
		{
			if (n == p)
			{
				n = nullptr;
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

	void level(int i) { _branch_level = i; };
	int level() { return _branch_level; };

	//void desc(std::atomic<QStack::Desc *> desc) { _desc = desc; };
	//std::atomic<QStack::Desc> desc() { return _desc; };

	std::atomic<QStack::Node *> _pred[MAX_FORK_AT_NODE] {nullptr};

private:
	T _val {NULL};
	Node *_next {nullptr};
	int _branch_level;
	int _predIndex = 0;
	bool _sentinel = false;
	Operation _type;
	int _opn;
	
};
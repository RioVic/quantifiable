#include <atomic>
#include <cstdio>
#include <ctime>
#include <math.h>
#include <vector>
#include <iostream>
#include <fstream>

#define CAS_LIMIT 3
#define MAX_THREADS 100
#define DELAY 2

void delay_clock(double dly){
    /* save start clock tick */
    const clock_t start = clock();

    clock_t current;
    do {
      // get current clock tick 
      current = clock();

      // break loop when the requested number of seconds have elapsed
    } while((double)(current-start)/CLOCKS_PER_SEC < dly);
}

template<typename T>
class QStack
{
	public:
		class Node;

		QStack(int num_threads, int num_ops) : 
		_stack(nullptr),
		data(num_threads)
		{
			for (int i = 0; i < MAX_THREADS; i++) 
			{
        		top.push_back(nullptr);
      		}

      		for (auto &v : data)
      		{
      			for (int i = 0; i < num_ops/num_threads; i++)
      			{
      				v.push_back(new Node());
      			}
      		}
		}

		bool push(int tid, int opn, T v);

    	bool pop(int tid, int opn, T &v);

    	void dumpNodes(std::ofstream &p);

	private:
    	std::atomic<Node *> _stack;
    	std::vector<Node *> top; // node pointer array for branches
    	std::vector<std::vector<Node *>> data; //Array for pre-allocated data
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

	int loop = 0;

	while (true) {

		//Read top of stack
	    Node *cur = _stack.load();
	    elem->next(cur);
	    Node *ttop = top[tid];
	    if (cur != nullptr) {
	    	//fprintf(stdout, "tid=%02i-%05i-%03i push INIT elem=%p elem->next()=%p, cur=%p cur->next()=%p cur->pred()=%p cur->value()=%c ttop=%p \n", \
	                       tid,    opn,  loop,       elem,   elem->next(),    cur,   cur->next(),   cur->pred(),   cur->value(),   ttop );
	    }

	    //fprintf(stdout, "tid=%02i-%05i-%03i push TTOP ", tid, opn, loop);
	    //for (int i=0; i < 16; i++) {
	    	//fprintf(stdout, "top%02i=%p ", i, top[i]);
	    //}
	    //fprintf(stdout, " \n");
	    //if (tid % 2 == 0) {
	    //	delay_clock(DELAY);
	    //}

	    // If we own a branch just push onto it as there is no contention
	    if (ttop != nullptr) {
	    	// elem becomes the top for this thread
	    	elem->next(ttop);
	    	top[tid] = elem;
	    	// Lazily set the pred pointer of the old top back to elem, the new node
	    	ttop->pred(elem);
	    	//fprintf(stdout, "tid=%02i-%05i-%03i push PRIV elem=%p elem->next()=%p, ttop=%p ttop->next()=%p ttop->pred()=%p ttop->value()=%c \n", \
	    	//                 tid,    opn,  loop,      elem,   elem->next(),    ttop,   ttop->next(),   ttop->pred(),   ttop->value() );
	    	return true;
	    } 
	    else 
	    {   
	    	// Else use the main branch _slack if it matches cur, then do a CAS elem the _stack
	    	if (_stack.compare_exchange_weak(cur, elem)) 
	    	{
	      		if (cur != nullptr) {
	        		// Lazily set the pred pointer back to elem, the new node
	        		cur->pred(elem);
	        		//fprintf(stdout, "tid=%02i-%05i-%03i push STAC elem=%p elem->next()=%p, cur=%p cur->next()=%p cur->pred()=%p cur->value()=%c \n", \
	        		//                 tid,    opn,  loop,     elem,   elem->next(),    cur,   cur->next(),   cur->pred(),   cur->value() );
	        	}
	        	return true;
	    	}
	    	// If we see a lot of contention and this is not the root of _stack, create our own multi-valued branch
	    	if ((cur != nullptr) && (ttop == nullptr) && (tid > 0)) 
	    	{
	    		//fprintf(stdout, "tid=%02i-%05i-%03i push CHEK checking CAS limit \n", tid, opn, loop);
	        	if (loop > CAS_LIMIT) {
	        		// elem becomes the top for this thread
	        		top[tid] = elem;
	        		// Lazily set the pred pointer back to elem, the new node
	        		cur->pred(elem);
	        		//fprintf(stdout, "tid=%02i-%05i-%03i push MULV elem=%p elem->next()=%p, cur=%p cur->next()=%p cur->pred()=%p cur->value()=%c \n", \
	        		//                 tid,     opn,  loop,     elem,   elem->next(),    cur,   cur->next(),   cur->pred(),   cur->value() );
	        		return true;
	    		}
	    	}  
	    } 
	    ++loop;
	}
}

template<typename T>
bool QStack<T>::pop(int tid, int opn, T& v) 
{
  	int loop = 0;

  	while (true) 
  	{
    	Node *cur = _stack.load();
		// --------- start Q algorithm here ----------
    	Node *next = nullptr;
    	Node *ttop = nullptr;
    	ttop = top[tid];

    	// If we have a private branch use it first
    	if (ttop != nullptr) 
    	{ 
      		next = ttop->next();
      		fprintf(stdout, "tid=%02i-%05i-%03i pop  PRIV cur=%p ttop=%p next=%p ttop->value()=%c \n", \
                       tid,     opn,  loop,    cur,   ttop,   next,   ttop->value() );
      		if (next->pred() == ttop) 
      		{ 
        		// top is the predecessor of next, so thread stays on this path
        		v = ttop->value();
        		//delete ttop;
        		top[tid] = next;
      		} 
      		else 
      		{ 
        		// set top[tid] to null and after this pop this thread will use _stack 
        		v = ttop->value();
        		//delete ttop;
        		top[tid] = nullptr;
      		}
      		return true;
    	}

    	// If there is a next node on the trunk, point to it
    	if (cur != nullptr) 
    	{
      		next = cur->next();
    	} 
    	else 
    	{
      		// Else the main branch is empty, so push into the pending pop stack
      		v = int('?');
      		return false;
    	}

    	fprintf(stdout, "tid=%02i-%05i-%03i pop  TTOP ", tid, opn, loop);
    	for (int i=0; i < 16; i++) 
    	{
      		fprintf(stdout, "top%02i=%p ", i, top[i]);
    	}
    	fprintf(stdout, " \n");
    	if (tid % 2 == 1) 
    	{
      		delay_clock(DELAY);
    	}

    	// If we are using the main branch of _stack do the classic CAS
    	if (_stack.compare_exchange_weak(cur, next)) 
    	{ 
      		fprintf(stdout, "tid=%02i-%05i-%03i pop  STAC cur=%p ttop=%p next=%p cur->value()=%c \n", \
                       tid,    opn,  loop, cur,   ttop,   next,   cur->value() );
      		v = cur->value();
      		//delete cur;
      		return true;
    	}
		//-----------------
  		++loop;
  	}
}

template<typename T>
void QStack<T>::dumpNodes(std::ofstream &p) 
{
	//Copy the thread branches and add the main branch
	std::vector<Node *> branches = this->top;
	branches.push_back(_stack);

	//Dump each branch
	for (auto *n : branches)
	{
		if (n == nullptr)
			continue;

		//Terminate when we reach the main branch, or when the main branch reaches the end
		do
		{
			p << "Address: " << n << "\tValue: " << n->value() << "\tNext: " << n->next() << "\n";
			n = n->next();
		} while (n->next() != nullptr && n->next()->pred() == n);
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

	private:
		T _val {NULL};
		Node *_next {nullptr};
		Node *_pred {nullptr};
};
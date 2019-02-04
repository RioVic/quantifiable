#include<stdio.h>
#include<stdlib.h>
#include<thread>
#include<atomic>
#include<chrono>
#include<cstring>
#include<ctime>
#include<time.h>

template<typename T> class LockFreeExchanger {
    std::atomic<uint64_t> slot;

    // unit64_t is compatible with pointer type
    uint64_t set(int state, T item = (T)NULL) {
        return ((uint64_t)item << 2) | state;
    }
    // Steal bits to make slot like Java Atomic Markable Reference
    int state(uint64_t slot) {
        return slot & 0b11;
    }

    T get(uint64_t slot) {
        return (T)(slot >> 2);
    }


public:
    LockFreeExchanger() {
        slot.store(0);
    }

    T exchange(T myItem) {
        uint64_t swap;
        int swap_state = EMPTY;
        clock_t timeLimit = clock() + ELIM_TIMEOUT;

        for (;;) {
            if (clock() > timeLimit) throw "timeout";
            swap = slot.load();

            switch (state(swap)) {

            case EMPTY:
                if (slot.compare_exchange_weak(swap, set(WAITING, myItem))) {
                    while (clock() < timeLimit) {
                        swap = slot.load();
                        if (state(swap) == BUSY) {
                            slot.store(set(EMPTY));
                            return get(swap);
                        }
                    }
                    swap = swap & WAITING;
                    if (slot.compare_exchange_weak(swap, set(EMPTY))) throw "timeout";
                    else {
                        swap = slot.load();
                        slot.store(set(EMPTY));
                        return get(swap);
                    }
                }
                break;

            case WAITING:
                if (slot.compare_exchange_weak(swap, set(BUSY, myItem)))
                    return get(swap);
                break;

         // case BUSY
            default: break;

            }
        }
    }
};


// The EliminationArray hods the pendng pops and pushes to be matched
// Herlihy and Shavit, Page 252

template<typename T> class EliminationArray {
private:
	LockFreeExchanger<T> **exchanger;

public:
	EliminationArray() {
		exchanger = new LockFreeExchanger<T>*[ELIM_CAPACITY]();
		for (int i = 0; i < ELIM_CAPACITY; i++)
			exchanger[i] = new LockFreeExchanger<T>();
		srand(102);
	}

	~EliminationArray() {
		for (int i = 0; i < ELIM_CAPACITY; i++)
			delete exchanger[i];
		delete[] exchanger;
	}

	T visit(T value) {
		return exchanger[rand() % ELIM_CAPACITY]->exchange(value);
	}
};


// The EliminationBackoffStack implements the data structure for a low contention stack
// Herlihy and Shavit, Page 253

template<class T> class EliminationBackoffStack {
private:
    struct Node {
		    T val;
		    Node *next;
	    };
    
	std::atomic<int> numPush, numPop, numSize;
	std::atomic<Node*> head;
		
	Node **nodeAlloc;
	Node root;	
	EliminationArray<T> *eliminationArray;

public:
	EliminationBackoffStack(int num_threads, int num_ops) {
        root.val = '?';
        root.next = NULL;
        head.store(&root);

		numPush = 0;
		numPop = 0;
		numSize = 0;
		eliminationArray = new EliminationArray<T>();

		nodeAlloc = new Node*[num_threads];

        for (int i = 0; i < num_threads; i++)
        {
            nodeAlloc[i] = new Node[num_ops];
        }
	}

	~EliminationBackoffStack()
    {
        for (int i = 0; i < num_threads; i++)
        {
            delete[] nodeAlloc[i];
        }
        delete[] nodeAlloc;
    }

	void push(int tid, int i, T x) 
	{
        ++numPush;
		Node *n = &nodeAlloc[tid][i];  // use allocated node
		n->val = x;

		for (;;) {
			n->next = head.load();
			if (head.compare_exchange_weak(n->next, n))
				return; 
			else try {
				if (!eliminationArray->visit(x))
					return; 
			} catch (const char *e) {
				continue;
			}
		}
	}

	T pop() 
	{
		++numPop;
		//fprintf(stdout, "      pop-----.6. \n");
		for (;;) {
			Node *t = head.load();
			//fprintf(stdout, "      pop-----.6.            t=%p t->val=%c \n", t, t->val);
            if (!(t->next)) return '?';  // returns ? for empty stack	
			if (head.compare_exchange_weak(t, t->next))
				return t->val;
			else try {
				T other = eliminationArray->visit((T)NULL);
				if (other)
					return other;
			} catch (const char *e) {
				continue;
			}
		}
	}

	int getSize() 
	{
		++numSize;
		return numPush-numPop;
	}

	int getNumOps() 
	{
		return numPush + numPop + numSize;
	}
};
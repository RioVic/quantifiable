// Victor Cook
// Experimental Work
//
// Quantifiable Stack vs
// Elimination Backoff Stack

#include<stdio.h>
#include<stdlib.h>
#include<thread>
#include<atomic>
#include<chrono>
#include<cstring>
#include<ctime>
#include <time.h>

// experimental parameters
#define MAX_THREADS 32       // max threads for all expeiments
#define MAX_OPS 100000000      // maximum number of operations on a single stack for naive memory maangement
#define ELIM_CAPACITY 10000 // capacity of elimination array
#define ELIM_TIMEOUT 1000
#define EMPTY 0
#define WAITING 1
#define BUSY 2

// command line args
int nThreads;             // upper limit for this experimental run
int nOpsPerThread;        // number of operations run per thread
int percentSize;          // percentage of size operations
int percentPush;          // percentage of push operations  
int percentPop;           // percentage of pop operations

int pseudoRand[MAX_OPS];  // repeatable pseudo random numbers to select operations
char c1, letters[53] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"; // data items


template<typename T> class AtomicMarkableReference  {
    std::atomic<uint64_t> amr;

    // unit64_t is compatible with pointer type
    uint64_t set(int state, T item = (T)NULL) {
        return ((uint64_t)item << 2) | state;
    }
    // Steal bits to make slot liek Java Atomic Markable Reference
    int state(uint64_t slot) {
        return amr & 0b11;
    }

    T get(uint64_t slot) {
        return (T)(amr >> 2);
    }

public:
    AtomicMarkableReference() {
        amr.store(0);
    }

};

class Treiber_S {
	struct Node {
	    char val;
	    Node *next;
	};
	Node nodeArray[MAX_THREADS+1][MAX_OPS];
	Node root, rootp;

    std::atomic<Node*> head, pending;
    std::atomic<int> numOps;
    std::atomic<int> size;
    std::atomic<int> spin;

    public:
        Treiber_S() {
            root.val = '?';
            root.next = NULL;
            head.store(&root);
            numOps = 0;
            size = 0;
        }

        bool push(int tid, int i, char item) {
            //fprintf(stdout, "tid=%i push%05i.1.    item=%c \n", tid, i, item);
            Node *t = head.load();
            Node *n = &nodeArray[tid][i];
            n->val = item; 
            //fprintf(stdout, "tid=%i push%05i.2.    item=%c t=%p t->val=%c n=%p n->val=%c \n", tid, i, item,  t, t->val, n, n->val);
            spin = 0; 
            do {
            	++spin;
                t = head.load();
                n->next = t;
            } while (!head.compare_exchange_weak(t, n));        
            numOps++;
            size++;
            //fprintf(stdout, "tid=%i push%05i.3.%03i item=%c t=%p t->val=%c n=%p n->val=%c \n", tid, i, item, spin.load(),  t, t->val, n, n->val);
            return true;
        }

        char pop() {
        	++numOps;
        	//fprintf(stdout, "     pop-----.1. \n");
            Node *t;
            Node *n;
            //fprintf(stdout, "     pop-----.2. \n");
            do {
                t = head.load();
                //fprintf(stdout, "     pop-----.3. \n");
                if (!(t->next)) return '?';  // returns ? for empty stack
                //fprintf(stdout, "     pop-----.4. \n");
                n = t->next;
                //fprintf(stdout, "     pop-----.5. \n");
            } while (!head.compare_exchange_weak(t, n));
            //numOps++;
            size--;
            //fprintf(stdout, "      pop-----.6.            t=%p t->val=%c n=%p n->val=%c \n", t, t->val, n, n->val);
            return t->val;
        }

        int getNumOps() {
            return numOps;
        }

        int getSize() {
        	//fprintf(stdout, "    size-----.1.   %05i \n", size.load());
            ++numOps;
            return size;
        }
};

void taskTS(Treiber_S *s, int tid, int nOps)
{
    int gs;
    char c;
    time_t tstart = time(NULL);
    int base = tid * nOps;

    for (int i = 0; i < nOps; i++) {
        if (pseudoRand[base+i] < percentSize) {
            gs = s->getSize();
            //fprintf(stdout, "taskT i=%i size=%i\n", tid, gs);
        }         
        else if (pseudoRand[base+i] < percentSize + percentPush) {
            s->push(tid, i, letters[pseudoRand[base+i]%26]); 
            //fprintf(stdout, "taskT i=%i push item=%c\n", tid, letters[pseudoRand[base+i]%26]); 
        }
        else {
            c = s->pop();
            //fprintf(stdout, "taskT i=%i pop ret=%c\n", tid, c );          
        }
        /*
        if (i%5 == 0) {
            time_t tnow = time(NULL);
            double elapsed = difftime(tnow, tstart);
            fprintf(stdout, "Thread %i iteration %i elapsed time %6.0f head %p \n", tid, i, elapsed, s);
        }
        */
    }
}

// The LockFreeExchanger allows two threads exchange values, eliminating each other's requests
// Herlihy and Shavit, Page 250

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
		
	Node nodeAlloc[MAX_THREADS+1][MAX_OPS];
	Node root;	
	EliminationArray<T> *eliminationArray;

public:
	EliminationBackoffStack() {
        root.val = '?';
        root.next = NULL;
        head.store(&root);

		numPush = 0;
		numPop = 0;
		numSize = 0;
		eliminationArray = new EliminationArray<T>();
	}

	void push(int tid, int i, T x) {
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

	T pop() {
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

	int getSize() {
		++numSize;
		return numPush-numPop;
	}

	int getNumOps() {
		return numPush + numPop + numSize;
	}
};

void taskEB(EliminationBackoffStack<char> *s, int tid, int nOps)
{
    int gs;
    char c;
    time_t tstart = time(NULL);

    for (int i = 0; i < nOps; i++) {
        if (pseudoRand[i] < percentSize) {
            gs = s->getSize();
            //fprintf(stdout, "%i size %i\n", tid, gs);
        }         
        else if (pseudoRand[i] < percentSize + percentPush) {
            s->push(tid, i, letters[pseudoRand[i]%26]); 
            //fprintf(stdout, "%i push %c\n", tid, letters[pseudoRand[i]%26]); 
        }
        else {
            c = s->pop();
            //fprintf(stdout, "%i pop %c\n", tid, c );          
        }
        /*
        if (i%5 == 0) {
            time_t tnow = time(NULL);
            double elapsed = difftime(tnow, tstart);
            fprintf(stdout, "Thread %i iteration %i elapsed time %6.0f head %p \n", tid, i, elapsed, s);
        }
        */
    }
}

int main(int argc, const char *argv[]) {
    if (argc < 5) {
       fprintf(stderr, "Please enter number of threads (1 to 32), percent each of size, push and pop. \n");
       return (5); 
    }
    nThreads = atoi(argv[1]);
    nOpsPerThread = atoi(argv[2]);
    percentSize = atoi(argv[3]);
    percentPush = atoi(argv[4]);
    percentPop = atoi(argv[5]);

    if (nThreads* nOpsPerThread > MAX_OPS) {
    	fprintf(stderr, "MAX_OPS=%i \n", MAX_OPS);
    	return(1);
    }

    char line[80];
    
    auto start = std::chrono::high_resolution_clock::now();

    std::thread worker[MAX_THREADS];
    srand(101);
    for (int i = 0; i < MAX_OPS; i++) {
       pseudoRand[i] = rand() % 100;
       //fprintf(stdout, "%i:%i ", i, pseudoRand[i]);
    }
    FILE *f = fopen("file.out", "w");
    fprintf(f, "\ntype,         mix,    threads, ms, ops\n");
    
    //fprintf(f, "\ntype,         mix,    threads, ticks, ops\n");

    // TREIBER STACK
    // run all thread counts for the given mix
    int N;   // number of threads in this run
    int t;   // index of thread
    for (N = 1; N <= nThreads; N++) {
        for (int jj = 0; jj < 10; jj++)
        {
            Treiber_S *tss = new Treiber_S();   
            start = std::chrono::high_resolution_clock::now();

            for (t = 0; t < N; t++) {
                worker[t] = std::thread(taskTS, tss, t, nOpsPerThread);
            }
            for (t = 0; t < N; t++){
                worker[t].join();
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = end-start;
            //fgets(line, sizeof(line), stdin);
            fprintf(f, "treiber_size\t%i-%i-%i\t%i\t%li\t%i\n", percentSize, percentPush, percentPop, N, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), tss->getNumOps());
            //fprintf(f, "treiber_size, %i-%i-%i, %i, %8.0f, %i\n", percentSize, percentPush, percentPop, N, elapsed, tss->getNumOps());
            delete tss;
        }
    }

    /* ELIMINATION STACK TEST
    EliminationStack<char> *ebs = new EliminationStack<char>(1000);

    start = time(NULL);  
    fprintf(stdout, "Elimination Stack... \n");
    fprintf(stdout, "head %p, numOps %i \n", ebs, ebs->getNumOps());

    t = 0;  
    for (int i = 0; i < 26; i++) {
        fprintf(stdout, "push item %c \n", letters[i]);
        ebs->push(t, i, letters[i]); 
    }
    fprintf(stdout, "head %p, numOps %i \n", ebs, ebs->getNumOps());

    for (int i = 0; i < 26; i++) {
        fprintf(stdout, "pop item %c \n", ebs->pop()); 

    }
    fprintf(stdout, "head %p, numOps %i \n", ebs, ebs->getNumOps());
    
    finish = time(NULL);
    elapsed = difftime(finish, start);
    fprintf(stdout, "short test time %.f \n", elapsed);
    */

    // ELIMINATION STACK 
    // run all thread counts for the given mix
    for (int N = 1; N <= nThreads; N++) {
        for (int jj = 0; jj < 10; jj++)
        {
            //fprintf(stdout, "N %i threads \n", N); 
            EliminationBackoffStack<char> *ebs = new EliminationBackoffStack<char>();
            auto start = std::chrono::high_resolution_clock::now();

            for (t = 0; t < N; t++) {
                //fprintf(stdout, "start thread %i \n", t);
                worker[t] = std::thread(taskEB, ebs, t, nOpsPerThread);
            }

            for (t = 0; t < N; t++){
                //fprintf(stdout, "join thread %i \n", t);
                worker[t].join();
            }

            //fprintf(stdout, "%i threads completed\n", N);
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = end-start;
            fprintf(f, "elim_backoff\t%i-%i-%i\t%i\t%li\t%i\n", percentSize, percentPush, percentPop, N, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), ebs->getNumOps());
            //fprintf(f, "elim_backoff, %i-%i-%i, %i, %8.0f, %i\n", percentSize, percentPush, percentPop, N, elapsed, ebs->getNumOps());
            delete ebs;
        }
    }

    //fclose(f);
    return (0);
}


#include<stdio.h>
#include<stdlib.h>
#include<thread>
#include<atomic>
#include<chrono>
#include<cstring>
#include<ctime>
#include<time.h>

class Treiber_S {
	struct Node 
    {
	    char val;
	    Node *next;
	};

	Node **nodeArray;
	Node root, rootp;

    std::atomic<Node*> head, pending;
    std::atomic<int> numOps;
    std::atomic<int> size;
    std::atomic<int> spin;

    public:
        Treiber_S(int num_threads, int num_ops) {
            root.val = '?';
            root.next = NULL;
            head.store(&root);
            numOps = 0;
            size = 0;

            nodeArray = new Node*[num_threads];

            for (int i = 0; i < num_threads; i++)
            {
                nodeArray[i] = new Node[num_ops];
            }
        }

        ~Treiber_S()
        {
            for (int i = 0; i < num_threads; i++)
            {
                delete[] nodeArray[i];
            }
            delete[] nodeArray;
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
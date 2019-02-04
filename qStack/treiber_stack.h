#include<stdio.h>
#include<stdlib.h>
#include<thread>
#include<atomic>
#include<chrono>
#include<cstring>
#include<ctime>
#include<time.h>

template<typename T>
class Treiber_S {
	struct Node 
    {
	    T val;
	    Node *next;
	};

	Node **nodeArray;
	Node root, rootp;

    std::atomic<Node*> head, pending;
    std::atomic<int> numOps;
    std::atomic<int> size;
    std::atomic<int> spin;

    int num_threads;

    public:
        Treiber_S(int num_threads, int num_ops) :
        num_threads(num_threads) 
        {
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

        bool push(int tid, int i, T item) {
            Node *n = &nodeArray[tid][i];
            n->val = item; 
            do {
                n->next = head.load();
            } while (!head.compare_exchange_weak(n->next, n));        
            return true;
        }

        bool pop(int tid, int opn, T &v) {
            Node *t;
            do {
                t = head.load();
                //Empty stack
                if (!(t->next))
                    return false;
            } while (!head.compare_exchange_weak(t, t->next));
            v = t->val;
            return true;
        }

        int getNumOps() {
            return numOps;
        }

        int getSize() {
            ++numOps;
            return size;
        }
};
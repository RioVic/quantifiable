#include <stdlib.h>
#include "delay.h"
#include "qqueue.h"
#include "primitives.h"

void queue_init(queue_t * q, int nprocs)
{
	q->nprocs = nprocs;
	q->head = malloc(q->nprocs * sizeof(node_t *));
	q->tail = malloc(q->nprocs * sizeof(node_t *));
	q->topDepths = align_malloc(PAGE_SIZE, sizeof(int) * nprocs);

	for (int i = 0; i < q->nprocs; i++)
	{
		q->topDepths[i] = 0;
		node_t *node = malloc(sizeof(node_t));
  		node->next = NULL;

  		q->head[i] = node;
  		q->tail[i] = node;
	}
}

void queue_register(queue_t * q, handle_t * th, int id)
{
  //hzdptr_init(&th->hzd, q->nprocs, 4);
  th->id = id;
}

void enqueue(queue_t * q, handle_t * handle, void * data)
{
	node_t *node = malloc(sizeof(node_t));

	node->data = data;
	node->next = NULL;
  	node->op = 1;

  	int index = handle->id;

  	node_t *tail;
  	node_t *tail_next;

  	while (1)
	{
		//Read the queue
		tail = q->tail[index];
		tail_next = tail->next;

		//Check if the queue is empty, or that there are no pending dequeue operations that need to be matched
		if (tail_next == NULL || tail->op == 1)
		{
			if (q->topDepths[index] > (q->topDepths[(index+1)%q->nprocs]) + 5
				|| q->topDepths[index] > (q->topDepths[(index-1)%q->nprocs]) + 5)
				continue;

			tail->next = node;
			q->tail[index] = node;
			q->topDepths[index]++;
			break;
		}
		else
		{
			node_t *head = q->head[index];
			q->head[index] = head->next;
			q->topDepths[index]--;
			free(head);
			break;
		}	
	}
}

void * dequeue(queue_t * q, handle_t * handle)
{
	int index = handle->id;
	void * data = NULL;

	while (1)
	{
		//Check if the queue if there are any nodes to dequeue, or that there are other dequeues waiting for a matching enqueue
		if (q->head[index]->next == NULL || q->tail[index]->op == 0)
		{
			//Add this dequeue operation to the queue as a pending operation
			node_t *node = malloc(sizeof(node_t));
			node->next = NULL;
			node->op = 0;

			q->tail[index]->next = node;
			q->tail[index] = node;
			q->topDepths[index]++;
			break;
		}
		else
		{
			node_t *head = q->head[index];
			data = head->next->data;
			q->head[index] = head->next;
			q->topDepths[index]--;
			free(head);
			break;
		}
	}

	return data;
}

void queue_free(int id, int nprocs) {}
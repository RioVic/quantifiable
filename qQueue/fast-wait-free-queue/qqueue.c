#include <stdlib.h>
#include "delay.h"
#include "qqueue.h"
#include "primitives.h"

void queue_init(queue_t * q, int nprocs)
{
}

void queue_register(queue_t * q, handle_t * th, int id)
{
  hzdptr_init(&th->hzd, q->nprocs, 4);
  th->id = id;
}

void enqueue(queue_t * q, handle_t * handle, void * data)
{
	node_t *node = malloc(sizeof(node_t));

	node->data = data;
  	node->op = 1;

  	int index = handle->id;

  	node_t *head;
  	node_t *tail;
  	node_t *head_next;
  	node_t *tail_next;

  	while (1)
	{
		//Read the queue
		tail = hzdptr_setv(&q->tail[index], &handle->hzd, 0);
		head = hzdptr_setv(&q->head[index], &handle->hzd, 1);
		tail_next = hzdptr_setv(&tail->next, &handle->hzd, 2);
		head_next = hzdptr_setv(&head->next, &handle->hzd, 3);

		//If tail_next is null, we must lazily catch up the tail pointer
		if (tail_next != NULL)
		{
			CAS(&q->tail[index], &tail, tail_next);
			continue;
		}

		//Check if the queue is empty, or that there are no pending dequeue operations that need to be matched
		if (head_next == NULL || tail->op == 1)
		{
			//Add our node to the list, update the tail pointer lazily
			if (CAS(&tail->next, &tail_next, node))
			{
				//Try once to update the tail pointer
				//If we fail, it means some other thread already did it
				CAS(&q->tail, &tail, node);
				break;
			}
		}
		else
		{
			//Remove the pending dequeue operation
			if (CAS(&q->head[index], &head, head_next))
			{
				//Node *head is no longer in the list, we can now retire it
				hzdptr_retire(&handle->hzd, head);
				break;
			}
		}	
	}
}

void * dequeue(queue_t * q, handle_t * handle)
{
	int index = handle->id;
	void * data;

	node_t *head;
  	node_t *tail;
  	node_t *head_next;
  	node_t *tail_next;

	while (1)
	{
		//Read the queue
		tail = hzdptr_setv(&q->tail[index], &handle->hzd, 0);
		head = hzdptr_setv(&q->head[index], &handle->hzd, 1);
		tail_next = hzdptr_setv(&tail->next, &handle->hzd, 2);
		head_next = hzdptr_setv(&head->next, &handle->hzd, 3);

		//If tail_next is null, we must lazily catch up the tail pointer
		if (tail_next != NULL)
		{
			CAS(&q->tail[index], &tail, tail_next);
			continue;
		}

		//Check if the queue if there are any nodes to dequeue, or that there are other dequeues waiting for a matching enqueue
		if (head_next == NULL || tail->op == 0)
		{
			//Add this dequeue operation to the queue as a pending operation
			node_t *node = malloc(sizeof(node_t));
			node->op = 0;

			//Add our node to the list, update the tail pointer lazily
			if (CAS(&tail->next, &tail_next, node))
			{
				//Try once to update the tail pointer
				//If we fail, it means some other thread already did it
				CAS(&q->tail, &tail, node);
				data = (void *) -1;
				break;
			}
		}
		else
		{
			data = head_next->data;

			//Remove the first node
			if (CAS(&q->head[index], &head, head_next))
			{
				//Node *head is no longer in the list, we can now retire it
				hzdptr_retire(&handle->hzd, head);
				break;
			}
		}
	}

	return data;
}

void queue_free(int id, int nprocs) {}
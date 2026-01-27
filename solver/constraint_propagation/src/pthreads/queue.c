#define THREADSAFE

#include "queue.h"
#include <string.h>
#include <pthread.h>
#include "commons.h"

#include "../../include/queue.h"
#include "../../include/commons.h"
#include "../../include/debug.h"


typedef struct Queue
{
	pthread_mutex_t mutex_lock;
	pthread_cond_t empty_condition;
	size_t size;
	size_t allocationSize;
	node *head;
	node *tail;
} queue;

queue *createQueue(size_t allocSize)
{
	DPRINTF("Creating Queue with data size: %ld\n", allocSize);
	queue *q = (queue *) malloc(sizeof(queue));
	if(q == NULL)
		return NULL;

	DPRINTF("Allocated queue: %ld\n",sizeof(queue));
	q->allocationSize = allocSize;
	q->size = 0;
	q->head = q->tail = NULL;

	int res = pthread_mutex_init(&(q->mutex_lock), NULL); 
	res = pthread_cond_init(&(q->empty_condition), NULL);
	return q;
}

queue *front(queue *q, void *data)
{
	if(q == NULL)
		return NULL;

	if(q->size == 0)
		return NULL;

	DPRINTF("Acquire lock %ld\n", q);
	acquire_lock(&(q->mutex_lock));
	memcpy(data, q->head->data, q->allocationSize);

	release_lock(&(q->mutex_lock));
	return q;
}

queue *reverse(queue *q)
{
	if(q == NULL)
		return NULL;

	if(q->size == 0)
		return q;	/* Nothing to reverse */

	void *data = malloc(q->allocationSize);

	acquire_lock(&(q->mutex_lock));
	if(data != NULL) {
		dequeue(q, data);
		reverse(q);
		enqueue(q, data);
		free(data);
	}

	release_lock(&(q->mutex_lock));
	return q;
}

queue *clearQueue(queue *q)
{
	if(q == NULL)
		return NULL;

	acquire_lock(&(q->mutex_lock));
	while(!isEmpty(q)) {
		node *temp = q->head;
		q->head = q->head->next;
		free(temp->data);
		free(temp);
		q->size--;
	}

	release_lock(&(q->mutex_lock));
	return q;
}

size_t getSize(queue *q)
{
	acquire_lock(&(q->mutex_lock));
	if(q == NULL)
		return 0;

	size_t size = q->size;
	release_lock(&(q->mutex_lock));
	
	return size;
}

bool isEmpty(queue *q)
{
	acquire_lock(&(q->mutex_lock));
	bool is_empty = q->size == 0 ? true : false;
	release_lock(&(q->mutex_lock));
	return is_empty;
}

size_t getAllocationSize(queue *q)
{
	if(q == NULL)
		return 0;

	return q->allocationSize;
}

queue *copyQueue(queue *src)
{
	if(src == NULL)
		return NULL;

	queue *newQueue = createQueue(src->allocationSize);
	if(newQueue == NULL)
		return NULL;

	acquire_lock(&(src->mutex_lock));

	/* Iterate through original queue and copy nodes */
	node *currentOriginalNode = src->head;
	node *previousNewNode = NULL;
	while(currentOriginalNode != NULL)
	{
		enqueue(newQueue, currentOriginalNode->data);
		currentOriginalNode = currentOriginalNode->next;
	}

	release_lock(&(src->mutex_lock));
	return newQueue;
}

void destroyQueue(queue **q)
{
	if(q == NULL)
		return;

	clearQueue(*q);
	free(*q);
	*q = NULL;
}

void *find(queue *q, bool (*predicate)(void *data))
{
	if(q == NULL || q->size == 0)
		return NULL;

	if(predicate == NULL)
		return NULL;

	node *current = q->head;

	while(current != NULL) {
		if(predicate(current->data))
			return current->data;

		current = current->next;
	}

	return NULL;
}

void *findMem(queue *q, void *data)
{
	if(q == NULL || data == NULL)
		return NULL;

	if(q->size == 0)
		return NULL;

	node *current = q->head;
	while(current != NULL) {
		if(memcmp(current->data, data, q->allocationSize) == 0)
			return current->data;

		current = current->next;
	}

	return NULL;
}

void _enqueue(queue *q,node *toInsert)
{
	if(q->size == 0)
	{ // First insertion
		q->head = q->tail = toInsert;
	}
	else
	{
		q->tail->next = toInsert;
		q->tail = toInsert;
	}

	q->size++;
    return;
}
node  *_dequeue(queue *q, void *data)
{
    while (q->size == 0)
    {
        condition_wait(&(q->empty_condition),&(q->mutex_lock));
    }
    node *toDel = q->head;
    if(q->size == 1)
    {
        q->head = q->tail = NULL;
    }
    else{
        q->head = q->head->next;
    }
    q->size--;
    return toDel;
}

queue *enqueue(queue *q, void *data)
{
    if(q == NULL || data == NULL)
    {
        return NULL;
    }

    node *toInsert = createNode(data, q->allocationSize);
    if(toInsert == NULL)
    {
        return NULL;
    }

    acquire_lock(&(q->mutex_lock));
    _enqueue(q,toInsert);
    release_lock(&(q->mutex_lock));
    condition_signal(&(q->empty_condition));

    return q;
}

queue *dequeue(queue *q, void *data)
{
    if(q == NULL)
    {
        return NULL;
    }
    acquire_lock(&(q->mutex_lock));
    node *toDel = _dequeue(q,data);
    release_lock(&(q->mutex_lock));
    memcpy(data, toDel->data, q->allocationSize);
    free(toDel->data);
    free(toDel);
    return q;
}

size_t batchDequeue(queue *q,void* data,size_t batch_size){
    
    if(q == NULL)
    {
        return 0;
    }

    if (batch_size == 0){
        return 0;
    }
    size_t size = batch_size;
    node* toDel_array;
    node* toDel;
    acquire_lock(&(q->mutex_lock));
    if (size > q->size)
        size = q->size;
    toDel_array = q->head;
    for(size_t i=0;i<size;i++)
    {
        q->head = q->head->next;
    }
    q->size=q->size-size;
    if (q->size == 0)
        q->tail=NULL;
    release_lock(&(q->mutex_lock));
    for (size_t i=0;i<size;++i ){
        toDel = toDel_array->next;
        memcpy(data+i*q->allocationSize, toDel_array->data, q->allocationSize);
        free(toDel_array->data);
        free(toDel_array);
        toDel_array=toDel;
    }
    return size;
}

queue *batchEnqueue(queue *q,void* data,size_t batch_size){
    if(q == NULL)
    {
        return NULL;
    }
    if (batch_size == 0){
        return q;
    }
    node** toInsert_array = batchNodeCreate(data,q->allocationSize,batch_size);
    acquire_lock(&(q->mutex_lock));
	if(q->size == 0)
	{ // First insertion
		q->head = toInsert_array[0];
        q->tail = toInsert_array[batch_size-1];
	}
	else
	{
		q->tail->next = toInsert_array[0];
		q->tail = toInsert_array[batch_size-1];
	}

	q->size=q->size+batch_size;
    release_lock(&(q->mutex_lock));
    return q;
}

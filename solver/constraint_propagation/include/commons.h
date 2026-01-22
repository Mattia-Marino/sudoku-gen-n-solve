#ifndef __COMMONS_H
#define __COMMONS_H
#include <assert.h>
#include <stdbool.h>
typedef struct Node
{
    void *data;
    struct Node *next;
} node;


typedef void* (*callback)(void *);
typedef unsigned int (*hash)(void*,unsigned int);
typedef void (*void_callback)(void *);
typedef bool (*comparator)(void *,void *);

node *createNode(void *data, size_t allocSize);

#define acquire_lock(lock)\
do { \
    assert( 0 == pthread_mutex_lock(lock));\
}while (0)

#define release_lock(lock)\
do { \
    assert( 0 == pthread_mutex_unlock(lock));\
}while (0)

#define condition_wait(cond,lock)\
do { \
    assert( 0 == pthread_cond_wait(cond,lock));\
}while (0)

#define condition_signal(cond)\
do { \
    assert( 0 == pthread_cond_signal(cond));\
}while (0)
#endif

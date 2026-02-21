#ifndef PTHREAD_GROUP_INCLUDED
#define PTHREAD_GROUP_INCLUDED 
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

struct t_info {
	pthread_t thread_id;
	int thread_num;
};

struct ThreadGroup {
	int groupId;
	unsigned int size;
	struct t_info t_comps[];
};

size_t get_threadgroup_size(struct ThreadGroup* tg);
struct ThreadGroup* create_thread_group(void* (**t_targets)(void *),void* restrict args[],size_t tn);
int join_thread_group(struct ThreadGroup* tg,void* outputs[]);
int cancel_thread_group(struct ThreadGroup* tg);
#endif

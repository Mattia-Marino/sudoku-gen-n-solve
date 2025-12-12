#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#define DEBUG
#include "debug.h"

#define EXIT_FAILURE 1
#define handle_error_en(en, msg) \
			 do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
			 do { perror(msg); exit(EXIT_FAILURE); } while (0)

int tg_count = 0; /*Keeps count of the created thread groups*/

typedef void* ( *target )( void *);

typedef struct ThreadInfo {
	pthread_t thread_id;
	int thread_num;
}t_info;

typedef struct ThreadGroup{
	int groupId;
	unsigned int size;
	t_info t_comps[];
}p_threadgroup;

static pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;
	p_threadgroup* create_thread_group(target t_targets[] ,void* restrict args[],size_t tn){
	size_t i;
	int ret;
	p_threadgroup *tg = malloc(sizeof(*tg) + sizeof( t_info)*tn);
	pthread_t tmp_thread;
	t_info tmp_info;

	pthread_mutex_lock(&count_lock);
	dprint("Creating threadgroup %d\n",tg_count);
	tg->groupId = tg_count++;
	tg->size = (unsigned int)tn;
	pthread_mutex_unlock(&count_lock);
	for(i=0; i<tn;i++){
		dprint("Creating thread number %ld\n",i);
		tmp_info.thread_num = i;
		ret = pthread_create(&tmp_thread,NULL,t_targets[i],(void*)(args+i));
		if (ret != 0){
			handle_error("pthread_create");
		}
		dprint("Created thread number %ld\n",i);
		tmp_info.thread_id = tmp_thread;
		tg->t_comps[i] = tmp_info;

	}
	return tg;
}


int cancel_thread_group( p_threadgroup* tg){
	size_t len = tg->size;
	size_t i;
	int res;
	
	for(i=0; i<len; i++){
		res = pthread_cancel(tg->t_comps[i].thread_id);
		if (res != 0) return res;
	}
	free(tg);
	return 0;
}

int join_thread_group( p_threadgroup* tg,void* outputs[]){
		size_t len = tg->size;
		size_t i;
		int res;

		for (i=0;i<len;++i){
			dprint("Internal arr: %ld \n",outputs[i]);
			res = pthread_join(tg->t_comps[i].thread_id,&outputs[i]);
			if (res!=0) return res;
		}
		free(tg);
		return 0;
}

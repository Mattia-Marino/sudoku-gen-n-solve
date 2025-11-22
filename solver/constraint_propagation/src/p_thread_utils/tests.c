#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "pthread_groups.h"
#include <unistd.h>

typedef void* ( *target )( void *);

void* test_func(void* arg){
	pthread_t self_id = pthread_self();
	printf("I'm executed inside the %lu thread\n",self_id);
	return NULL;
}


int main(int args, char* argv[]){
	printf("Main started\n");
	target t_targets[3];
	int my_args = {1,2,3};
	t_targets[0] = &test_func;
	t_targets[1] = &test_func;
	t_targets[2] = &test_func;
	struct ThreadGroup* tg = create_thread_group(t_targets,my_args,3);
	unsigned int size = tg->size;
	printf("Tg size %u\n",size);
	join_thread_group(tg);
/**/
	printf("End of main\n");
	return 0;
}

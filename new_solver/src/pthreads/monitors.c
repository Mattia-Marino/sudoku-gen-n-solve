#include "../../monitors.h"


void init_rw_monitor(struct RW_monitor *monitor){
    pthread_mutex_init(&monitor->entry,NULL);
    pthread_cond_init(&monitor->write,NULL);
    pthread_cond_init(&monitor->read,NULL);
    monitor->nw = 0;
    monitor->nr = 0;
    monitor->dw = 0;
}

void rw_monitor_request_read(struct RW_monitor *monitor){
    pthread_mutex_lock(&monitor->entry);
    while(monitor->nw > 0 || monitor->dw > 0)
        pthread_cond_wait(&monitor->read,&monitor->entry);
    monitor->nr++;
    pthread_mutex_unlock(&monitor->entry);
}

void rw_monitor_request_write(struct RW_monitor *monitor){
    pthread_mutex_lock(&monitor->entry);
    monitor->dw++;
    while(monitor->nw > 0 || monitor->nr > 0)
        pthread_cond_wait(&monitor->write,&monitor->entry);
    monitor->nw++;
    monitor->dw--;
    pthread_mutex_unlock(&monitor->entry);
}

void rw_monitor_release_read(struct RW_monitor *monitor){
    pthread_mutex_lock(&monitor->entry);
    monitor->nr--;
    if (monitor->nr == 0)
        pthread_cond_signal(&monitor->write);
    pthread_mutex_unlock(&monitor->entry);
}

void rw_monitor_release_write(struct RW_monitor *monitor){
    pthread_mutex_lock(&monitor->entry);
    monitor->nw--;
    pthread_cond_broadcast(&monitor->read);
    pthread_cond_signal(&monitor->write);
    pthread_mutex_unlock(&monitor->entry);
}

void rw_monitor_destroy(struct RW_monitor *monitor){
    pthread_cond_destroy(&monitor->read);
    pthread_cond_destroy(&monitor->write);
    pthread_mutex_destroy(&monitor->entry);
}

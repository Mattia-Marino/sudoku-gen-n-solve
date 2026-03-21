#ifndef PTHREAD_MONITORS
#define PTHREAD_MONITORS
#include <pthread.h>

// Invariant (nr == 0 or nw == 0) & nw <= 1 
struct RW_monitor
{
    pthread_mutex_t entry; // Monitor entry exclusion
    pthread_cond_t write; // Signaled on nw == 0
    pthread_cond_t read; // Signaled on nw == 0 and nr == 0
    // Active processes number
    unsigned int nw;
    unsigned int nr;
    unsigned int dw; // Delayed writer processes number (used to avoid writer starvation)
};

void init_rw_monitor(struct RW_monitor *monitor);
void rw_monitor_request_read(struct RW_monitor *monitor);
void rw_monitor_request_write(struct RW_monitor *monitor);
void rw_monitor_release_read(struct RW_monitor *monitor);
void rw_monitor_release_write(struct RW_monitor *monitor);
void rw_monitor_destroy(struct RW_monitor *monitor);

#endif

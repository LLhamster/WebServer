#ifndef PHREAD_H
#define PTHREAD_H
#include <pthread.h>
#include <stdio.h>

class pthread{
private:
    static int const WORKER_NUM = 4;
    static pthread_t workers_[WORKER_NUM];
    static pthread_mutex_t lock_;
    static pthread_cond_t cond_;
    static int front_, rear_;
    static int queue_[1024];
public:
    static int pthread_init(int epollfd);
    static void* worker_(void* arg);
    static int dqueue();
    static void enque(int fd);

};   


#endif
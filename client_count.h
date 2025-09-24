#ifndef CLIENT_COUNT_H
#define CLIENT_COUNT_H
#include <pthread.h>

extern volatile int client_count;
extern pthread_mutex_t client_count_mutex;

#endif // CLIENT_COUNT_H

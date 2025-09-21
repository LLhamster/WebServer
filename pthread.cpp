#include "pthread.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util.h"
#include <stdint.h>

pthread_mutex_t pthread::lock_ = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pthread::cond_ = PTHREAD_COND_INITIALIZER;
int pthread::front_ = 0;
int pthread::rear_ = 0;
pthread_t pthread::workers_[WORKER_NUM];
int pthread::queue_[1024];


static void drain_read(int fd) {
    char buf[4096];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) continue;
        if (n == 0) break; // 对端关闭
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        break;
    }
}


int pthread::pthread_init(int epollfd){
    for(int j = 0; j < WORKER_NUM; j++){
        pthread_create(&workers_[j], NULL, worker_, (void*)(intptr_t)epollfd);
    }
    return 0;
}

void* pthread::worker_(void* arg){
        while(1){
            int work_fd = dqueue();
            const char *header =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Transfer-Encoding: chunked\r\n"
                "Connection: close\r\n\r\n";
            write(work_fd, header, strlen(header));
            // 第一个 chunk：11(0xB) 字节 + CRLF
            write(work_fd, "C\r\n你好世界\r\n", strlen("C\r\n你好世界\r\n")); // 11
            // 第二个 chunk：3(0x3) 字节 + CRLF
            write(work_fd, "3\r\nHHH\r\n", strlen("3\r\nHHH\r\n")); // 8
            // 结束 chunk
            write(work_fd, "0\r\n\r\n", strlen("0\r\n\r\n")); // 5
            shutdown(work_fd, SHUT_WR);
            drain_read(work_fd);
            close(work_fd);
            }
        return NULL;
}

int pthread::dqueue() {
    pthread_mutex_lock(&lock_);
    while(front_ == rear_){
        pthread_cond_wait(&cond_, &lock_);
    }
    int fd = queue_[front_];
    front_ = (front_ + 1) % 1024;
    pthread_mutex_unlock(&lock_);
    return fd;
}

void pthread::enque(int fd) {
    pthread_mutex_lock(&lock_);
    queue_[rear_] = fd;
    rear_ = (rear_ + 1)%1024;
    pthread_cond_signal(&cond_);
    pthread_mutex_unlock(&lock_);
}
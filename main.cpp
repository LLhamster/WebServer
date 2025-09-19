#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/epoll.h>
#include "util.h"
#include <pthread.h>

// 简单线程池
int const WORKER_NUM = 4;
pthread_t workers[WORKER_NUM];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int front = 0, rear = 0;
int queue[1024];
int epollfd = -1;

void enque(int fd) {
    pthread_mutex_lock(&lock);
    queue[rear] = fd;
    rear = (rear + 1)%1024;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

int dqueue() {
    pthread_mutex_lock(&lock);
    while(front == rear){
        pthread_cond_wait(&cond, &lock);
    }
    int fd = queue[front];
    front = (front + 1) % 1024;
    pthread_mutex_unlock(&lock);
    return fd;
}

void* worker(void* arg){
    while(1){
        int work_fd = dqueue();
        const char *header =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Transfer-Encoding: chunked\r\n\r\n";
        (void)write(work_fd, header, strlen(header));
        // 第一个chunk (长度 11 = "Hello world")
        (void)write(work_fd, "B\r\nHello world\r\n", 15);
        sleep(5);
        // 第二个chunk (长度 3 = "HHH")
        (void)write(work_fd, "3\r\nHHH\r\n", 7);
        // 结束chunk
        (void)write(work_fd, "0\r\n\r\n", 5);
        // struct epoll_event* work_event;
        // work_event->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        // work_event->data.fd = work_fd;
        // epoll_ctl(epollfd, EPOLL_CTL_ADD, work_fd, work_event);
        close(work_fd);
}
    return NULL;
}


int main(int argc,char *argv[]) {
    //判断命令行参数个数是否正确
    if(argc < 3) {
        printf("usage:%s ip_address port number\n",argv[0]);
        return 1;
    }
    //将字符串形式的端口号转换为整数，并进行错误检查
    char *endptr;
    int port = strtol(argv[2], &endptr, 10);
    if(*endptr != '\0') {
        printf("usage:%s ip_address must be number\n", argv[0]);
        return 1;
    }
    //创建一个套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd > 0);
    if(setnoblock(sockfd) == -1){
        printf("setnoblock error\n");
        close(sockfd);
        return 1;
    }
    
    //命名套接字
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(argv[1]);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("bind error\n");
        close(sockfd);
        return 1;
    }
    //监听套接字
    if(listen(sockfd, 1) != 0) {
        printf("listen error\n");
        close(sockfd);
        return 1;
    }
    //创建epoll对象,并将监听套接字添加到epoll对象中
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    struct epoll_event event[1024];
    __uint32_t events = EPOLLIN | EPOLLET;
    event->events = events;
    event->data.fd = sockfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, event);//将监听套接字添加到epoll对象中
    assert(epollfd != -1);
    //创建工作线程
    for(int j = 0; j < WORKER_NUM; j++){
        pthread_create(&workers[j], NULL, worker, (void*)(intptr_t)epollfd);
    }
    
    //接受连接
    while(1){
        int ret = epoll_wait(epollfd, event, 1024, -1);
        if(ret < 0) {
            printf("epoll failure\n");
            break;
        }
        else if(ret == 0) {
            printf("epoll timeout\n");
            continue;
        }
        else{
            for(int i = 0; i<ret; i++){
                if(event[i].data.fd == sockfd){
                    //创建新的连接描述符
                    struct sockaddr_in client;
                    socklen_t client_addrlength = sizeof(client);
                    while(1){
                        int connfd = accept(sockfd, (struct sockaddr*)&client, &client_addrlength);
                        if(connfd < 0) {
                            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                            {
                                break;
                            }
                            else{
                                perror("accept");
                                break;

                            }

                        }
                        if(setnoblock(connfd) == -1){
                            printf("setnoblock error\n");
                            close(sockfd);
                            return 1;
                        }
                        events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                        event->events = events;
                        event->data.fd = connfd;
                        epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, event);
                    }
                }
                else{
                    enque(event[i].data.fd);
                }
            }

        }
    }
    close(sockfd);
    return 0;
}
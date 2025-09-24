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
#include "epoll.h"
#include "pthread.h"
#include <signal.h>
#include "client_count.h"

// 全局客户端计数及互斥锁
typedef volatile int atomic_int;
atomic_int client_count = 0;
pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;

int socket_bind_listen(int port) {
    //创建一个套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    assert(setnoblock(sockfd) != -1);
    //命名套接字
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("bind error\n");
        close(sockfd);
        return -1;
    }
    //监听套接字
    if(listen(sockfd, 128) != 0) {
        perror("listen error\n");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int main(int argc,char *argv[]) {
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    //判断命令行参数个数是否正确
    if(argc < 3) {
        printf("usage:%s ip_address port number\n",argv[0]);
        return 1;
    }
    //将字符串形式的端口号转换为整数，并进行错误检查
    char *endptr;
    int port = strtol(argv[2], &endptr, 10);
    assert(*endptr == '\0');

    //创建一个socket
    int sockfd = socket_bind_listen(port);
    assert(sockfd != -1);
    
    //创建epoll对象,并将监听套接字添加到epoll对象中
    int epollfd = epoll::epoll_init();
    if(epoll::epoll_add(sockfd)<0){
        perror("epoll_add error");
        return 1;
    }
    
    //创建工作线程
    assert(pthread::pthread_init(epollfd) == 0);
    
    //接受连接
    while(1){
        printf("wait for new connection...\n");
        int result = epoll::epoll_wait_work(sockfd);
        // 这里仅演示如何安全打印当前连接数，实际加减应在accept/close处
        pthread_mutex_lock(&client_count_mutex);
        printf("当前客户端连接数: %d\n", client_count);
        pthread_mutex_unlock(&client_count_mutex);
        // printf("result=%d\n", result);
        if(result < 0){
            printf("epoll wait error\n");
            break;      
        }
    }
    close(sockfd);
    return 0;
}
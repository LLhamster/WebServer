#include "epoll.h"
#include <sys/epoll.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "pthread.h"

uint32_t epoll::events_ = 0;
struct epoll_event epoll::event_[1024];

int epoll::epollfd_ = -1;

int epoll::epoll_init() {
    epollfd_ = epoll_create(5);
    assert(epollfd_ != -1);
    return epollfd_;
}

int epoll::epoll_add(int sockfd) {
    events_ = EPOLLIN | EPOLLET;
    event_[0].events = events_;
    event_[0].data.fd = sockfd;
    if(epoll_ctl(epollfd_, EPOLL_CTL_ADD, sockfd, &event_[0])<0){
        perror("epoll_add error");
        return -1;
    }//将监听套接字添加到epoll对象中
    return 0;
}

int epoll::epoll_wait_work(int sockfd) {
    int ret = epoll_wait(epollfd_, event_, 1024, -1);
        if(ret < 0) {
            printf("epoll failure\n");
            return -1;
        }
        else{
            for(int i = 0; i<ret; i++){
                if(event_[i].data.fd == sockfd){
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
                        // if(setnoblock(connfd) == -1){
                        //     printf("setnoblock error\n");
                        //     close(sockfd);
                        //     return 1;
                        // }
                        // struct epoll_event workEvent;
                        // __uint32_t workEvents;
                        // workEvents = EPOLLIN | EPOLLET | EPOLLONESHOT;
                        // workEvent.events = workEvents;
                        // workEvent.data.fd = connfd;
                        // epoll_ctl(epollfd_, EPOLL_CTL_ADD, connfd, &workEvent);
                        pthread::enque(connfd);
                        printf("new connection");
                    }
                }
                // else{
                //     pthread::enque(event_[i].data.fd);
                // }
            }

        }
        return 0;
}


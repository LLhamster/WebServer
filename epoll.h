#ifndef EPOLL_H
#define EPOLL_H
#include <cstdint>

class epoll{
private:
    static int epollfd_;
    static struct epoll_event event_[1024];
    static uint32_t events_;

public:
    static int epoll_init();
    static int epoll_add(int sockfd);
    static int epoll_wait_work(int sockfd);

};

#endif
#include "util.h"
#include <fcntl.h>

int setnoblock(int fd){
    int flag = fcntl(fd, F_GETFD,0);
    if(flag == -1){
        return -1;
    }
    flag |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flag) == -1){
        return -1;
    }
    return 0;
}
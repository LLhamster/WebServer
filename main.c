#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>


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
    if(listen(sockfd, 5) != 0) {
        printf("listen error\n");
        close(sockfd);
        return 1;
    }
    //接受连接
    while(1){
        struct sockaddr_in client;
        socklen_t client_addrlength = sizeof(client);
        int connfd = accept(sockfd, (struct sockaddr*)&client, &client_addrlength);
        if(connfd < 0) {
            printf("accept error\n");
            close(sockfd);
            return 1;
        } else {
            const char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nHello world";
            write(connfd, msg, strlen(msg));
            close(connfd);
        }
    }
    
    close(sockfd);
    return 0;
}
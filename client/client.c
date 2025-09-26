/*
 * 客户端：
 *      连接服务端
 *      接收服务端发来的“hello”消息即可
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <error.h>
#include <sys/stat.h>

#define BUFFERMAX 60

int main(int argc, char *argv[]) {
    char buffer[BUFFERMAX] = {0};
    char* serverip = "10.211.55.5";
    char* port = "8563";
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(port));
    server.sin_addr.s_addr = inet_addr(serverip);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    if (connect(sockfd, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    ssize_t ret_recv = recv(sockfd, buffer, BUFFERMAX, 0);
    if (ret_recv == -1) {
        perror("recv");
    }else if (ret_recv > 0){
        buffer[ret_recv] = '\0';
        printf("Client received data: %s\n", buffer);
        close(sockfd);
    }
    return 0;
}
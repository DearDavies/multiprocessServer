#include "head.h"

int init_socket(int* socket_fd, char* ip, char* port) {
    struct sockaddr_in local_server = {0};
    local_server.sin_family = AF_INET;
    local_server.sin_port = htons(atoi(port));
    local_server.sin_addr.s_addr = inet_addr(ip);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("init_socket：socket");
        return -1;
    }
    // 设置端口重用
    int reuseaddr = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));
    // 绑定
    if (bind(sockfd, (struct sockaddr*)&local_server, sizeof(struct sockaddr)) == -1) {
        perror("init_socket：bind");
        return -1;
    }
    // 监听
    if (listen(sockfd, 10) == -1) {
        perror("init_socket：listen");
        return -1;
    }
    *socket_fd = sockfd;
    return 0;
}

/*
 * 功能：设置监听事件，包括 sockfd 和 池中每个子进程的 local_sockpair_fd。
 * 返回值：错误返回非零值。
 */
int add_epoll(int epoll_fd, int event_fd, client_t* clients, int len) {
    if (epoll_fd < 0) {
        perror("epoll_fd 小于 0");
        return -1;
    }
    if (clients != NULL && len <= 0) {
        perror("len <= 0 ");
        return -1;
    }
    if (event_fd < 0 && clients == NULL) {
        perror("至少要监听一个有效的读事件");
        return -1;
    }
    struct epoll_event event;
    event.events = EPOLLIN;
    if (clients != NULL) {
        for (int i = 0; i < len; i++) {
            event.data.fd = clients[i].local_sockpair_fd;
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clients[i].local_sockpair_fd, &event);
        }
    }
    if (event_fd > 0) {
        event.data.fd = event_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &event);
    }
    return 0;
}

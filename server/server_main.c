#include <signal.h>
#include <bits/signum-generic.h>

#include "head.h"

// 注册一对主进程写、主进程读的管道，用来自己通知自己接收到了中断信号
// 目前定义为全局变量，因为 catch_SIGINT 要使用
int pipefd[2];

void catch_SIGINT(int sig) {
    printf("主进程捕捉到信号：SIGINT\n");
    write(pipefd[1], "2", 1);
}

int main() {
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // 注册捕捉2号信号的函数
    signal(SIGINT, catch_SIGINT);

    // 数组，用来存储子进程客户端的状态
    client_t clients[MAXCLIENTS] = {0};

    // 用来接收消息的缓存
    char buffer[BUFFERMAX] = {0};

    // 用来接收函数的执行结果
    int ret = 0;
    int sockfd = 0;

    char* local_ip = "0.0.0.0";
    char* port = "8563";

    if (init_socket(&sockfd, local_ip, port) != 0) {
        perror("init_socket error");
        exit(EXIT_FAILURE);
    }

    // 创建 epoll
    int epoll_fd = epoll_create(1);
    if (epoll_fd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    if (add_epoll(epoll_fd, pipefd[0],NULL, 0) != 0) {
        perror("添加管道监听事件错误");
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[MAXCLIENTS] = {0};

    // 初始化客户端池：
    // 创建子进程，并设置子进程的状态数组
    ret = init_client_pool(clients, MAXCLIENTS);
    if (ret != 0) {
        perror("init_client_pool");
        exit(EXIT_FAILURE);
    }

    /*
     * 设置主进程的所有监听事件，包括 sockfd 和 池中每个子进程的 local_sockpair_fd
     */
    if (add_epoll(epoll_fd, sockfd, clients, MAXCLIENTS) != 0) {
        printf("添加读事件监听错误：sockfd = %d, 四个子进程\n", sockfd);
        exit(EXIT_FAILURE);
    }

    /*
     * 前期 server 的准备工作已完毕
     * server 的工作：
     *      1. 监听 sockfd ，如果来了新连接，交给空闲的子进程执行，
     *      2. 监听子进程的 local_sockpaird_fd，如果子进程的任务执行完毕，则修改子进程在数组中的状态
     */
    while (1) {
        int ret_epoll = epoll_wait(epoll_fd, events, MAXCLIENTS, -1);
        if (ret_epoll == -1) {
            /*
             * 当 epoll_wait() 正在阻塞等待事件时，主进程捕捉到 SIGINT 信号，
             * 并且这个信号的处理函数被成功执行并返回后，
             * epoll_wait() 会被中断，随即返回 -1，同时将 errno 设置为 EINTR。
             */
            if (errno == EINTR) {
                continue;
            }
            // 发生了真正的错误
            perror("epoll_wait 发生了错误");
            exit(EXIT_FAILURE);
        }
        if (ret_epoll == 0) {
            perror("epoll_wait 超时了");
        }
        else {
            for (int i = 0; i < ret_epoll; i++) {
                int event_fd = events[i].data.fd;

                // 如果是 sockfd，说明有新的客户端连接来了
                if (event_fd == sockfd) {
                    printf("有新的客户端已连接！\n");
                    int new_client_fd = accept(sockfd, NULL, NULL);
                    if (new_client_fd == -1) {
                        perror("接收新客户端出错了");
                    }
                    else {
                        // 找到空闲的子进程，并把新客户的连接交给子进程
                        ret = dispath_client_pool(new_client_fd, clients, MAXCLIENTS);
                        // 返回值不为 0，则发生错误
                        if (ret != 0) {
                            // 如果所有子进程都在忙，则返回 -1。服务端给客户端发送“目前服务器正忙”，并断开连接
                            if (ret == -1) {
                                perror("dispath_client_pool");
                                char warning_message[] = "目前服务器正忙";
                                send(new_client_fd, warning_message, sizeof(warning_message), 0);
                            }
                        }
                        close(new_client_fd);
                    }
                }
                // 如果是管道来消息了，说明主进程接收到了 SIGINT
                else if (event_fd == pipefd[0]) {
                    read(pipefd[0], buffer, BUFFERMAX);
                    printf("正在将退出信号发送给每个各个子进程\n");
                    for (int i = 0; i < MAXCLIENTS; i++) {
                        printf("正在发送给 %d 号子进程\n", i + 1);
                        send_to_worker(clients[i].local_sockpair_fd, 0, 1);
                        wait(NULL);
                    }
                    printf("主进程即将结束\n");
                    exit(0);
                }
                // 否则就是子进程来消息
                else {
                    //////////////////////////////////////////////////////////////////
                    // 这段代码是接收子进程传来的通知信息
                    // 必须要接收，因为 epoll 默认是水平触发模式
                    // 如果不接收信息，那么会导致 epoll_wait 不断返回，致使 BUG 出现
                    recv(event_fd, &buffer, BUFFERMAX, 0);
                    //////////////////////////////////////////////////////////////////

                    // 找到是哪个子进程的信息，更新客户端数组的信息
                    for (int i = 0; i < MAXCLIENTS; i++) {
                        if (event_fd == clients[i].local_sockpair_fd) {
                            clients[i].status = FREE;
                            break;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

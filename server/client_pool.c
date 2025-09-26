#include "head.h"

/*
 * 功能：初始化客户端池，创建子进程，并设置子进程的状态数组。
 * 返回值：错误返回非零值。
 */
int init_client_pool(client_t* clients, int len) {
    for (int i = 0; i < len; i++) {
        // 创建用于父子进程通信的 sockpair
        int sv[2];
        if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sv) == -1) {
            perror("socketpair failed");
            return -1;
        }
        pid_t pid = fork();
        if (pid < 0) {
            perror("init_client_pool：fork");
            return -1;
        }
        // 子进程
        if (pid == 0) {
            close(sv[0]);
            if (setpgid(0,0) < 0) {
                perror("init_client_pool：setpgid failed");
                exit(EXIT_FAILURE);
            }
            printf("%d 号子进程（pid = %d）进入后台进程组（pgid = %d）\n", i + 1, getpid(), getpgid(getpid()));

            int sockpair_fd = sv[1];
            int ret = son_main(sockpair_fd);
            if (ret != 0) {
                printf("son_main 的错误码是：%d\n", ret);
                exit(EXIT_FAILURE);
            }
        }
        // 父进程
        else {
            close(sv[1]);
            int sockpair_fd = sv[0];
            clients[i].pid = pid;
            clients[i].status = FREE;
            clients[i].local_sockpair_fd = sockpair_fd;
        }
    }
    return 0;
}

/*
 * 功能：找到空闲的子进程，并把新的客户连接交给子进程。
 * 返回值：错误返回非零值。如果所有子进程都在忙，则返回 -1；
 *                     如果发送新客户端连接失败，则返回 2；
 *                     如果参数不正确，则返回 1；
 */
int dispath_client_pool(int new_client_fd, client_t* clients, int len) {
    if (clients == NULL || len <= 0 || new_client_fd < 0) {
        return 1;
    }
    for (int i = 0; i < len; i++) {
        if (clients[i].status == FREE) {
            int ret = send_to_worker(clients[i].local_sockpair_fd, new_client_fd, 0);
            if (ret != 0) {
                return 2;
            }
            clients[i].status = BUSY;
            close(new_client_fd);
            return 0;
        }
    }
    return -1;
}


/*
 * 功能：子进程的执行主函数。
 * 参数：与主进程用来通信的 socketpair
 * 返回值：错误返回非零值。
 *                      epoll_create = 1
 *                      子进程设置读监听出错 = 2
 *                      子进程执行epoll_wait出错 = 3
 *                      子进程等待超时
 *                      子进程接收新客户端连接出错 = 5
 *                      子进程添加新客户端监听出错 = 6
 *                      son_main：send to client = 7
 *                      son_main：send to master = 8
 */
int son_main(int sockpair_fd) {
    int epoll_fd = epoll_create(1);
    if (epoll_fd == -1) {
        perror("epoll_create");
        return 1;
    }
    int ret = 0;
    ret = add_epoll(epoll_fd, sockpair_fd);
    if (ret != 0) {
        perror("子进程设置读监听出错");
        return 2;
    }
    struct epoll_event events[2] = {0};

    while (1) {
        int ret_epoll = epoll_wait(epoll_fd, events, 2, -1);
        switch (ret_epoll) {
            case -1:
                perror("子进程执行epoll_wait出错");
                return 3;
            case 0:
                perror("子进程等待超时");
                break;
            default:
                for (int i = 0; i < ret_epoll; i++) {
                    // 如果是 sockpair_fd，说明来新客户端连接
                    if (events[i].data.fd == sockpair_fd) {
                        int recved_fd = 0;
                        ret = recv_from_master(sockpair_fd, &recved_fd);
                        if (ret != 0) {
                            printf("子进程接收新客户端连接出错\n");
                            return 5;
                        }
                        ret = add_epoll(epoll_fd, recved_fd);
                        if (ret != 0) {
                            printf("子进程添加新客户端监听出错\n");
                            return 6;
                        }

                        /*
                         * 下面是子进程执行的任务
                         */
                        // 给客户端发送一个 hello
                        // printf("子进程(pid = %d)给客户端发送一个 hello\n", getpid());
                        if (send(recved_fd,"hello", 5, MSG_NOSIGNAL) == -1) {
                            perror("son_main：send to client");
                            return 7;
                        }

                        sleep(5);

                        // 任务完成，给主进程发送完成信息，(并删除客户端连接的监听)
                        // printf("子进程(pid = %d)给父进程(ppid = %d)发送完成通知\n", getpid(), getppid());
                        if (send(sockpair_fd, "done", 4, MSG_NOSIGNAL) == -1) {
                            perror("son_main：send to master");
                            return 8;
                        }
                    }
                    // 客户端来的信息
                    else {
                        int client_fd = events[i].data.fd;
                        char buffer[BUFFERMAX] = {0};
                        ret = recv(client_fd, buffer, BUFFERMAX, 0);
                        if (ret == 0) {
                            printf("客户端已断开\n");
                            ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            // printf("子进程 pid = %d 从 epoll = %d 中 删除事件（%d），ret = %d \n", getpid(), epoll_fd, client_fd,ret);
                            close(client_fd);
                        }
                        else {
                            printf("客户端发来了消息：%s\n", buffer);
                        }
                    }
                }
        }
    }
    return 0;
}
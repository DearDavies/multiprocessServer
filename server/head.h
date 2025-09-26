#ifndef HEAD_H
#define HEAD_H

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
#include <sys/wait.h>

#define BUFFERMAX 60
#define MAXCLIENTS 4

// 用来表示子进程的状态：空闲或繁忙
typedef enum {
    FREE,
    BUSY
} status_t;

/*
 * 用来存储子进程的各种信息
 *
 */
typedef struct {
    pid_t pid;
    int local_sockpair_fd;
    status_t status;
} client_t;

/*
 * 功能：初始化客户端池，创建子进程，并设置子进程的状态数组。
 * 返回值：错误返回非零值。
 */
int init_client_pool(client_t* clients, int len);

/*
 * 功能：设置主进程的所有监听事件，包括 sockfd 和 池中每个子进程的 local_sockpair_fd。
 * 返回值：错误返回非零值。
 */
int add_epoll(int epoll_fd, int event_fd, client_t* clients, int len);

/*
 * 功能：找到空闲的子进程，并把新的客户连接交给子进程。
 * 返回值：错误返回非零值。如果所有子进程都在忙，则返回 -1；
 *                     如果发送新客户端连接失败，则返回 2；
 *                     如果参数不正确，则返回 1；
 */
int dispath_client_pool(int new_client_fd, client_t* clients, int len);

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
int son_main(int sockpair_fd);

/*
 * 功能：父进程通过 sockpair_fd，把父进程中的 new_client_fd 传递给子进程
 * 返回值：错误返回非零值
 */
int send_to_worker(int local_sockpair_fd, int new_client_fd, int exit_flag);

/*
 * 功能：子进程接收父进程通过 sockpair_fd，发来的 new_client_fd
 * 返回值：错误返回非零值
 */
int recv_from_master(int sockpair_fd, int* recved_fd);


int init_socket(int* socket_fd, char* ip, char* port);

void catch_SIGINT(int sig);



#endif //HEAD_H

#include "head.h"

/*
 * 功能：父进程通过 sockpair_fd，把父进程中的 new_client_fd 传递给子进程
 * 返回值：错误返回非零值
 */
int send_to_worker(int local_sockpair_fd, int new_client_fd, int exit_flag) {
    if (local_sockpair_fd < 0 || new_client_fd < 0) {
        return -1;
    }

    char buffer[40] = {0};
    char* normal_message = "This is a normal data payload.";
    char* exit_message = "This is a exit signal.";

    // --- 准备 sendmsg ---
    struct msghdr msg = {0};
    if (exit_flag == 0) {
        memcpy(buffer, normal_message, strlen(normal_message));
    }
    else {
        memcpy(buffer, exit_message, strlen(exit_message));
    }

    // a. 准备普通数据 (iovec)
    struct iovec iov[1];
    iov[0].iov_base = buffer;
    iov[0].iov_len = strlen(buffer);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    // b. 准备辅助数据 (control message) 来存放文件描述符
    // CMSG_SPACE 计算存放一个 int (文件描述符) 所需的控制消息缓冲区的总大小
    char cmsg_buffer[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buffer;
    msg.msg_controllen = sizeof(cmsg_buffer);

    // c. 填充控制消息头 (cmsghdr)
    // CMSG_FIRSTHDR(&msg)：查看 msg ，找到 msg.msg_control 字段里记录的地址（也就是 cmsg_buffer ）
    // 然后返回一个指向该地址的、类型为 struct cmsghdr * 的指针
    // 不直接强转赋值，因为要具备更好的健明性、可移植性和长期维护性
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET; // 协议级别
    cmsg->cmsg_type = SCM_RIGHTS; // 消息类型，SCM_RIGHTS 表示这是一个文件描述符
    cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // 控制消息的总长度

    if (exit_flag == 0) {
        // d. 将文件描述符复制到控制消息的数据部分
        *((int*)CMSG_DATA(cmsg)) = new_client_fd;
    }

    // 4. 发送消息
    if (sendmsg(local_sockpair_fd, &msg, MSG_NOSIGNAL) == -1) {
        perror("sendmsg failed");
        return -1;
    }
    return 0;
}

/*
 * 功能：子进程接收父进程通过 sockpair_fd，发来的 new_client_fd
 * 返回值：错误返回非零值
 */
int recv_from_master(int sockpair_fd, int* recved_fd) {
    // --- 准备 recvmsg ---
    struct msghdr msg = {0};
    char recv_buffer[40];
    char* normal_message = "This is a normal data payload.";
    char* exit_message = "This is a exit signal.";

    // a. 准备接收普通数据的 iovec
    struct iovec iov[1];
    iov[0].iov_base = recv_buffer;
    iov[0].iov_len = sizeof(recv_buffer);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    // b. 准备接收辅助数据的缓冲区
    char cmsg_buffer[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buffer;
    msg.msg_controllen = sizeof(cmsg_buffer);

    // 6. 接收消息
    ssize_t n = recvmsg(sockpair_fd, &msg, 0);
    if (n <= 0) {
        perror("recvmsg failed");
        return -1;
    }

    // 如果接收到的消息是退出消息，说明子进程该结束了
    if (strcmp((char*)msg.msg_iov[0].iov_base, exit_message) == 0) {
        printf("子进程（pid = %d）将要结束。\n", getpid());
        exit(0);
    }

    if (strcmp((char*)msg.msg_iov[0].iov_base, normal_message) == 0) {
        // printf("Child: Received normal data: '%.*s'\n", (int)n, (char*)msg.msg_iov[0].iov_base);

        // 7. 从控制消息中提取文件描述符
        int received_fd = -1;
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);

        // 必须进行严格检查
        if (cmsg != NULL && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                received_fd = *((int*)CMSG_DATA(cmsg));
                // printf("Child: Successfully extracted fd: %d\n", received_fd);
            }
        }
        if (received_fd == -1) {
            perror("recvmsg failed");
            return -1;
        }
        *recved_fd = received_fd;
        return 0;
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_SIZE 1024

pid_t child_pid;

void handle_shutdown(int sig) {
    if (child_pid > 0) {
        kill(child_pid, SIGKILL);
    }
    printf("\n服务器正在关闭...\n");
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "用法: %s <端口号>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // 1. 创建套接字
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket 创建失败");
        exit(1);
    }

    // 2. 绑定地址和端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind 失败");
        exit(1);
    }

    // 3. 监听端口
    if (listen(listen_fd, 5) < 0) {
        perror("listen 失败");
        exit(1);
    }

    printf("服务器已启动，正在监听端口 %d...\n", port);

    // 4. 接受客户端连接
    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) {
        perror("accept 失败");
        exit(1);
    }
    close(listen_fd); // 关闭监听套接字，只处理一个客户端
    printf("客户端 %s:%d 已连接。\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // 设置信号处理，用于父进程退出
    signal(SIGINT, handle_shutdown); 

    child_pid = fork();

    if (child_pid < 0) {
        perror("fork 失败");
        exit(1);
    }

    if (child_pid == 0) { // 子进程: 从标准输入读取并发送
        while (1) {
            fgets(buffer, BUFFER_SIZE, stdin);
            send(conn_fd, buffer, strlen(buffer), 0);
            if (strncmp(buffer, "exit", 4) == 0) {
                // 如果服务器端输入exit，也通知客户端并关闭
                kill(getppid(), SIGINT); // 通知父进程关闭
                break;
            }
        }
    } else { // 父进程: 接收并显示消息
        ssize_t n;
        while ((n = recv(conn_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            buffer[n] = '\0';
            printf("客户端: %s", buffer);

            // 服务器如果收到客户方的exit文本，则退出服务器程序
            if (strncmp(buffer, "exit", 4) == 0) {
                printf("收到客户端的退出指令，服务器关闭。\n");
                kill(child_pid, SIGKILL); // 结束子进程
                break;
            }
        }
        if (n == 0) {
            printf("客户端已断开连接。\n");
            kill(child_pid, SIGKILL);
        }
        wait(NULL); // 等待子进程结束
    }

    close(conn_fd);
    return 0;
}
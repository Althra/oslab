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
    printf("\n正在断开连接...\n");
    exit(0);
}

int main(int argc, char* argv[]) {
    // 命令行要求有服务器地址，服务器端口号
    if (argc != 3) {
        fprintf(stderr, "用法: %s <服务器IP> <端口号>\n", argv[0]);
        exit(1);
    }

    const char* server_ip = argv[1];
    int port = atoi(argv[2]);
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // 1. 创建套接字
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket 创建失败");
        exit(1);
    }

    // 2. 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("无效的服务器地址");
        exit(1);
    }

    // 3. 连接服务器
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect 连接失败");
        exit(1);
    }

    printf("成功连接到服务器 %s:%d。可以开始聊天了。\n", server_ip, port);
    
    signal(SIGINT, handle_shutdown);

    child_pid = fork();
    
    if (child_pid < 0) {
        perror("fork 失败");
        exit(1);
    }

    if (child_pid == 0) { // 子进程: 接收并显示消息
        ssize_t n;
        while ((n = recv(sock_fd, buffer, BUFFER_SIZE, 0)) > 0) {
            buffer[n] = '\0';
            printf("服务器: %s", buffer);
        }
        if (n == 0) {
            printf("服务器已断开连接。\n");
            kill(getppid(), SIGINT); // 通知父进程退出
        }
    } else { // 父进程: 从标准输入读取并发送
        while (1) {
            fgets(buffer, BUFFER_SIZE, stdin);
            send(sock_fd, buffer, strlen(buffer), 0);
            
            // 客户方通过输入exit文本结束与服务器的通信并退出程序
            if (strncmp(buffer, "exit", 4) == 0) {
                kill(child_pid, SIGKILL); // 结束子进程
                break;
            }
        }
        wait(NULL);
    }

    close(sock_fd);
    return 0;
}
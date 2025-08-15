#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_SIZE 4096
#define WEB_ROOT "./webroot"

void error_die(const char *msg) { perror(msg); exit(1); }
const char* get_mime_type(const char* filename);
void send_404(int client_sock);
void send_file_response(int client_sock, const char* local_path);
void handle_client(int client_sock);

// SIGCHLD 信号处理函数，用于回收僵尸进程
void sigchld_handler(int sig) {
    // 即使没有子进程退出，也会立即返回，防止阻塞
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "用法: %s <端口号>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 注册 SIGCHLD 信号处理函数
    signal(SIGCHLD, sigchld_handler);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) error_die("socket 创建失败");

    // 允许地址重用
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error_die("bind 失败");
    }

    if (listen(server_sock, 10) < 0) {
        error_die("listen 失败");
    }

    printf("Web服务器已启动，正在监听端口 %d...\n", port);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("accept 失败");
            continue;
        }

        // 创建子进程来处理连接
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork 失败");
        } else if (pid == 0) {
            // 子进程不需要监听套接字
            close(server_sock); 
            // 处理请求
            handle_client(client_sock);
            // 处理完毕，子进程退出
            exit(0);
        } else {
            // 父进程不需要连接套接字
            close(client_sock); 
            // 继续循环，等待下一个连接
        }
    }

    close(server_sock);
    return 0;
}

const char* get_mime_type(const char* filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    return "application/octet-stream";
}

void send_404(int client_sock) {
    const char *response = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n"
                           "<html><body><h1>404 Not Found</h1></body></html>";
    send(client_sock, response, strlen(response), 0);
}

void send_file_response(int client_sock, const char* local_path) {
    char header_buf[1024];
    struct stat file_stat;
    
    if (stat(local_path, &file_stat) < 0 || S_ISDIR(file_stat.st_mode)) {
        send_404(client_sock);
        return;
    }

    const char *mime_type = get_mime_type(local_path);
    sprintf(header_buf, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n",
            mime_type, file_stat.st_size);
    send(client_sock, header_buf, strlen(header_buf), 0);

    int file_fd = open(local_path, O_RDONLY);
    if (file_fd < 0) return;
    
    char file_buf[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, file_buf, sizeof(file_buf))) > 0) {
        send(client_sock, file_buf, bytes_read, 0);
    }
    close(file_fd);
}

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    char method[16], path[256];
    
    ssize_t bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("--- 收到来自 PID %d 的请求 ---\n%s\n", getpid(), buffer);
        
        if (sscanf(buffer, "%s %s", method, path) == 2 && strcmp(method, "GET") == 0) {
            if (strcmp(path, "/") == 0) strcpy(path, "/index.html");
            char local_path[512];
            snprintf(local_path, sizeof(local_path), "%s%s", WEB_ROOT, path);
            send_file_response(client_sock, local_path);
        }
    }
    close(client_sock);
}
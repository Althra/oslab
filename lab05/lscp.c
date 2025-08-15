#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

// 递归遍历并执行拷贝的函数
void traverse_and_copy(const char *src_dir, const char *dest_dir) {
    DIR *dir = opendir(src_dir);
    if (!dir) {
        fprintf(stderr, "错误: 无法打开目录 '%s'\n", src_dir);
        perror("");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // 忽略 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_src_path[1024];
        snprintf(full_src_path, sizeof(full_src_path), "%s/%s", src_dir, entry->d_name);

        struct stat statbuf;
        if (lstat(full_src_path, &statbuf) < 0) {
            perror("lstat 失败");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // 如果是目录，则递归调用
            printf("父进程: 发现子目录 '%s', 递归遍历...\n", full_src_path);
            traverse_and_copy(full_src_path, dest_dir);
        } else if (S_ISREG(statbuf.st_mode)) {
            // 如果是文件，则fork一个子进程来拷贝
            pid_t pid = fork();
            
            if (pid < 0) {
                perror("fork 失败");
            } else if (pid == 0) {
                // 子进程
                char full_dest_path[1024];
                snprintf(full_dest_path, sizeof(full_dest_path), "%s/%s", dest_dir, entry->d_name);
                
                printf("子进程 (PID: %d): 开始执行 './mycp %s %s'\n", getpid(), full_src_path, full_dest_path);
                
                // 执行 mycp 程序
                execl("./mycp", "mycp", full_src_path, full_dest_path, (char *)NULL);

                // 如果失败，则打印错误并退出
                perror("子进程: execl 失败");
                exit(1);

            } else {
                // 父进程
                int status;
                // 等待刚刚创建的那个子进程结束
                waitpid(pid, &status, 0); 
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    printf("父进程: 子进程 %d 已成功完成拷贝。\n", pid);
                } else {
                    fprintf(stderr, "父进程: 子进程 %d 执行失败。\n", pid);
                }
            }
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    char source_dir[1024];
    char destination_dir[] = "/home/zsh/"; // 固定目标目录

    // 解析命令行参数
    if (argc > 2) {
        fprintf(stderr, "用法: %s [源目录]\n", argv[0]);
        return 1;
    } else if (argc == 2) {
        strncpy(source_dir, argv[1], sizeof(source_dir));
    } else {
        if (getcwd(source_dir, sizeof(source_dir)) == NULL) {
            perror("getcwd 失败");
            return 1;
        }
    }

    // 验证源路径是否为目录
    struct stat path_stat;
    if (stat(source_dir, &path_stat) != 0 || !S_ISDIR(path_stat.st_mode)) {
        fprintf(stderr, "错误: '%s' 不是一个有效的目录。\n", source_dir);
        return 1;
    }

    // 创建目标目录，如果已存在则忽略错误
    if (mkdir(destination_dir, 0755) && errno != EEXIST) {
        perror("无法创建目标目录");
        return 1;
    }
    
    printf("源目录: %s\n", source_dir);
    printf("目标目录: %s\n", destination_dir);
    printf("-----------------------------------------\n");

    traverse_and_copy(source_dir, destination_dir);

    printf("-----------------------------------------\n");
    printf("所有文件拷贝完成。\n");
    return 0;
}
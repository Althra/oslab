#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#define BUF_SIZE 4096

void copy_file(const char* src, const char* dst) {
    int src_fd, dst_fd;
    int open_flags;
    mode_t file_perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 默认文件权限 0644

    // 检查源文件是否存在且可读
    src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        fprintf(stderr, "错误: 无法打开源文件 '%s': ", src);
        perror("");
        return;
    }

    // 检查目标文件是否存在
    if (access(dst, F_OK) == 0) {
        // 目标文件存在，询问用户
        char choice;
        printf("目标文件 '%s' 已存在。请选择操作: [o]覆盖, [a]合并(追加), [c]取消: ", dst);
        choice = getchar();
        while (getchar() != '\n'); 

        switch (choice) {
            case 'o':
            case 'O':
                // O_TRUNC 在写入前清空文件内容，实现覆盖
                open_flags = O_WRONLY | O_TRUNC;
                printf("...正在覆盖 '%s'\n", dst);
                break;
            case 'a':
            case 'A':
                // O_APPEND 在文件末尾追加内容，实现合并
                open_flags = O_WRONLY | O_APPEND;
                printf("...正在追加到 '%s'\n", dst);
                break;
            case 'c':
            case 'C':
                printf("...操作已取消。\n");
                close(src_fd);
                return;
            default:
                printf("...无效的选项。操作已取消。\n");
                close(src_fd);
                return;
        }
        dst_fd = open(dst, open_flags);

    } else {
        // 目标文件不存在，直接创建
        open_flags = O_WRONLY | O_CREAT;
        dst_fd = open(dst, open_flags, file_perms);
    }
    
    if (dst_fd < 0) {
        fprintf(stderr, "错误: 无法打开目标文件 '%s': ", dst);
        perror("");
        close(src_fd);
        return;
    }

    // 执行文件内容的读写拷贝
    char* buffer = malloc(BUF_SIZE);
    if (!buffer) {
        perror("错误: 分配缓冲区失败");
        close(src_fd);
        close(dst_fd);
        exit(1);
    }

    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(src_fd, buffer, BUF_SIZE)) > 0) {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("错误: 写入目标文件时发生错误");
            break; 
        }
    }

    // 清理资源
    free(buffer);
    close(src_fd);
    close(dst_fd);

    if (bytes_read < 0) {
        fprintf(stderr, "错误: 读取源文件 '%s' 时发生错误: ", src);
        perror("");
    }
}


void copy_directory(const char* src, const char* dst){
    DIR *dir = opendir(src);
    if (!dir) {
        fprintf(stderr, "错误: 无法打开源目录 '%s': ", src);
        perror("");
        return;
    }

    // 确保目标目录存在，如果不存在则创建
    struct stat st;
    if (stat(dst, &st) < 0) {
        if (mkdir(dst, 0755) < 0) {
            fprintf(stderr, "错误: 无法创建目标目录 '%s': ", dst);
            perror("");
            closedir(dir);
            return;
        }
    }

    struct dirent *entry;
    struct stat statbuf;
    char src_path[1024], dst_path[1024];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);
        
        if (lstat(src_path, &statbuf) < 0) {
            fprintf(stderr, "错误: 无法获取 '%s' 的属性: ", src_path);
            perror("");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            copy_directory(src_path, dst_path);
        } else if (S_ISREG(statbuf.st_mode)) {
            copy_file(src_path, dst_path);
        }
    }

    closedir(dir);
}

int main(int argc,char *argv[]) {
    char* source = NULL;
    char* destination = NULL;
    bool is_recursive = false;

    // 参数解析
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-R") == 0) {
            is_recursive = true;
        } else if (source == NULL) {
            source = argv[i];
        } else if (destination == NULL) {
            destination = argv[i];
        }
    }

    if (source == NULL || destination == NULL) {
        fprintf(stderr, "用法: %s [-r] <源> <目标>\n", argv[0]);
        return 1;
    }

    struct stat src_stat, dst_stat;
    // 源文件不存在
    if (lstat(source, &src_stat) < 0) {
        fprintf(stderr, "错误: 源 '%s' 不存在或无法访问: ", source);
        perror("");
        return 1;
    }

    if (S_ISREG(src_stat.st_mode)) {
        if (stat(destination, &dst_stat) == 0 && S_ISDIR(dst_stat.st_mode)) {
            // 文件到目录
            char dst_file[1024];
            const char *filename = strrchr(source, '/');
            filename = filename ? filename + 1 : source; // 提取文件名
            snprintf(dst_file, sizeof(dst_file), "%s/%s", destination, filename);
            copy_file(source, dst_file);
        } else {
            // 文件到文件
            copy_file(source, destination);
        }
    } else if (S_ISDIR(src_stat.st_mode)) {
        if (!is_recursive) {
            fprintf(stderr, "错误: 源 '%s' 是一个目录, 请使用 -r 选项进行递归拷贝。\n", source);
            return 1;
        }

        struct stat dst_stat;
        char final_destination[1024];

        if (stat(destination, &dst_stat) == 0) {
            // 目标存在
            if (S_ISDIR(dst_stat.st_mode)) {
                // 如果目标是目录，则构建新路径: "destination/source_basename"
                const char *source_basename = strrchr(source, '/');
                source_basename = source_basename ? source_basename + 1 : source;
                snprintf(final_destination, sizeof(final_destination), "%s/%s", destination, source_basename);
            } else {
                // 如果目标存在但不是目录，报错
                fprintf(stderr, "错误: 目标 '%s' 已存在且不是一个目录。\n", destination);
                return 1;
            }
        } else {
            // 目标不存在，直接使用用户提供的目标路径
            strncpy(final_destination, destination, sizeof(final_destination));
        }
        copy_directory(source, final_destination);
    } else {
        fprintf(stderr, "错误: 不支持的源文件类型。\n");
        return 1;
    }

    printf("拷贝操作完成。\n");
    return 0;
}
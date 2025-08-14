#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>

// 存储命令行选项的状态
int show_all = 0;
int dereference_link = 0;

// 格式化并打印文件权限
void print_permissions(mode_t mode) {
    if (S_ISDIR(mode)) putchar('d');
    else if (S_ISLNK(mode)) putchar('l');
    else putchar('-');

    putchar((mode & S_IRUSR) ? 'r' : '-');
    putchar((mode & S_IWUSR) ? 'w' : '-');
    putchar((mode & S_IXUSR) ? 'x' : '-');
    putchar((mode & S_IRGRP) ? 'r' : '-');
    putchar((mode & S_IWGRP) ? 'w' : '-');
    putchar((mode & S_IXGRP) ? 'x' : '-');
    putchar((mode & S_IROTH) ? 'r' : '-');
    putchar((mode & S_IWOTH) ? 'w' : '-');
    putchar((mode & S_IXOTH) ? 'x' : '-');
    printf(" ");
}

// 处理单个文件并打印其详细信息
void display_file_info(const char *dir_path, const char *filename) {
    struct stat file_stat;
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, filename);

    int stat_result = dereference_link ? stat(full_path, &file_stat) : lstat(full_path, &file_stat);
    if (stat_result < 0) {
        fprintf(stderr, "无法获取 '%s' 的属性: ", full_path);
        perror("");
        return;
    }

    print_permissions(file_stat.st_mode);
    printf("%3ld ", (long)file_stat.st_nlink);

    struct passwd *pw = getpwuid(file_stat.st_uid);
    if (pw) printf("%-8s ", pw->pw_name);
    else printf("%-8d ", file_stat.st_uid);

    struct group *gr = getgrgid(file_stat.st_gid);
    if (gr) printf("%-8s ", gr->gr_name);
    else printf("%-8d ", file_stat.st_gid);

    printf("%10lld ", (long long)file_stat.st_size);

    char time_buf[80];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", localtime(&file_stat.st_mtime));
    printf("%s ", time_buf);

    printf("%s", filename);

    if (S_ISLNK(file_stat.st_mode)) {
        char link_target[1024];
        ssize_t len = readlink(full_path, link_target, sizeof(link_target) - 1);
        if (len != -1) {
            link_target[len] = '\0';
            printf(" -> %s", link_target);
        }
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    char *dir_path_arg = NULL;

    // 1. 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') { // 选项
            for (char *p = argv[i] + 1; *p; p++) {
                if (*p == 'a') show_all = 1;
                else if (*p == 'L') dereference_link = 1;
            }
        } else { // 路径参数
            if (dir_path_arg != NULL) {
                fprintf(stderr, "错误: 只能指定一个目录路径。\n");
                exit(EXIT_FAILURE);
            }
            dir_path_arg = argv[i];
        }
    }

    // 2. 检查是否提供了目录路径参数
    if (dir_path_arg == NULL) {
        fprintf(stderr, "错误: 未指定目录路径。\n");
        fprintf(stderr, "用法: %s [选项] <目录路径>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 3. 验证路径是否为有效目录
    struct stat path_stat;
    if (stat(dir_path_arg, &path_stat) != 0) {
        perror(dir_path_arg);
        exit(EXIT_FAILURE);
    }
    if (!S_ISDIR(path_stat.st_mode)) {
        fprintf(stderr, "错误: '%s' 不是一个目录。\n", dir_path_arg);
        exit(EXIT_FAILURE);
    }

    // 4. 打开并遍历目录
    DIR *dir = opendir(dir_path_arg);
    if (dir == NULL) {
        perror(dir_path_arg);
        exit(EXIT_FAILURE);
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }
        display_file_info(dir_path_arg, entry->d_name);
    }

    closedir(dir);
    return 0;
}
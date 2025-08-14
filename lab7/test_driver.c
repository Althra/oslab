#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/process_lister"

int main() {
    int fd;
    char buffer[10];

    printf("--> Starting test...\n");

    // 打开设备文件，触发驱动中的 my_open() 函数
    printf("--> 1. Opening %s ...\n", DEVICE_PATH);
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device.\n");
        return 1;
    }
    printf("--> Device opened successfully.\n\n");

    // 读取设备，触发驱动中的 my_read() 函数
    printf("--> 2. Reading device (triggering my_read)...\n");
    read(fd, buffer, sizeof(buffer));
    printf("--> Read operation completed.\n\n");

    // 写入设备，触发驱动中的 my_write() 函数
    printf("--> 3. Writing device (triggering my_write)...\n");
    write(fd, "hello", 5);
    printf("--> Write operation completed.\n\n");

    // 关闭设备，触发驱动中的 my_release() 函数
    printf("--> 4. Closing device...\n");
    close(fd);
    printf("--> Device closed.\n");

    printf("\n--> Test completed.\n");

    return 0;
}
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#define IPC_KEY 0x9876
#define BUFFER_SIZE 5
#define MAX_STR_LEN 100
#define CONSUMER_TIMEOUT 5 // 消费者等待超时时间（秒）

// 信号量索引
#define SEM_MUTEX 0
#define SEM_EMPTY 1
#define SEM_FULL  2

// 共享内存中的缓冲池结构
struct BufferPool {
    char buffers[BUFFER_SIZE][MAX_STR_LEN];
    int write_pos;
    int read_pos;
};

// 信号量操作所需的联合体
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

// P操作（等待）
int semaphore_p(int sem_id, int sem_no) {
    struct sembuf sem_b = {sem_no, -1, SEM_UNDO};
    if (semop(sem_id, &sem_b, 1) == -1) {
        perror("semaphore_p failed");
        return -1;
    }
    return 0;
}

// V操作（释放）
int semaphore_v(int sem_id, int sem_no) {
    struct sembuf sem_b = {sem_no, 1, SEM_UNDO};
    if (semop(sem_id, &sem_b, 1) == -1) {
        perror("semaphore_v failed");
        return -1;
    }
    return 0;
}

// 带超时的P操作
int sem_timed_p(int sem_id, int sem_no, int timeout_sec) {
    struct sembuf sem_b = {sem_no, -1, SEM_UNDO};
    struct timespec timeout = {timeout_sec, 0};
    if (semtimedop(sem_id, &sem_b, 1, &timeout) == -1) {
        return (errno == EAGAIN) ? -2 : -1; // -2 表示超时
    }
    return 0;
}

// 生产者子进程执行的逻辑
void run_producer(int id, int shmid, int semid) {
    char filename[32];
    sprintf(filename, "producer%d.txt", id);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Producer: fopen failed");
        exit(1);
    }

    struct BufferPool *pool = (struct BufferPool *)shmat(shmid, NULL, 0);
    char line[MAX_STR_LEN];
    printf("--- 生产者 %d (PID %d) 启动, 读取文件: %s\n", id, getpid(), filename);

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0; // 移除换行符

        semaphore_p(semid, SEM_EMPTY); // 等待空位
        semaphore_p(semid, SEM_MUTEX); // 锁定

        strcpy(pool->buffers[pool->write_pos], line);
        printf("生产者 %d -> 缓冲区[%d]: \"%s\"\n", id, pool->write_pos, line);
        pool->write_pos = (pool->write_pos + 1) % BUFFER_SIZE;

        semaphore_v(semid, SEM_MUTEX); // 解锁
        semaphore_v(semid, SEM_FULL);  // 通知有新产品
        sleep(1);
    }

    fclose(fp);
    shmdt(pool);
    printf("--- 生产者 %d 完成文件读取, 退出\n", id);
}

// 消费者子进程执行的逻辑
void run_consumer(int id, int shmid, int semid) {
    char filename[32];
    sprintf(filename, "consumer%d.txt", id);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Consumer: fopen failed");
        exit(1);
    }
    
    struct BufferPool *pool = (struct BufferPool *)shmat(shmid, NULL, 0);
    char buffer[MAX_STR_LEN];
    printf("--- 消费者 %d (PID %d) 启动, 写入文件: %s\n", id, getpid(), filename);

    while (1) {
        printf("消费者 %d 正在等待产品...\n", id);
        int ret = sem_timed_p(semid, SEM_FULL, CONSUMER_TIMEOUT);
        
        if (ret == 0) { // 成功等到产品
            semaphore_p(semid, SEM_MUTEX);
            strcpy(buffer, pool->buffers[pool->read_pos]);
            pool->read_pos = (pool->read_pos + 1) % BUFFER_SIZE;
            semaphore_v(semid, SEM_MUTEX);
            semaphore_v(semid, SEM_EMPTY);

            printf("消费者 %d <- 缓冲区: \"%s\"\n", id, buffer);
            fprintf(fp, "%s\n", buffer);
            fflush(fp);

        } else if (ret == -2) { // 等待超时
            printf("\n>>> 消费者 %d 等待超时 (%d 秒). 是否继续等待? (y/n): ", id, CONSUMER_TIMEOUT);
            int response = getchar();
            // 清理输入缓冲区中多余的字符
            while (response != '\n' && getchar() != '\n'); 
            
            if (response != 'y' && response != 'Y') {
                printf("--- 消费者 %d 退出\n", id);
                break;
            }
        } else { // 发生其他错误
            break;
        }
    }

    fclose(fp);
    shmdt(pool);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法: %s <生产者数量> <消费者数量>\n", argv[0]);
        exit(1);
    }
    int num_producers = atoi(argv[1]);
    int num_consumers = atoi(argv[2]);

    // 1. 创建临时的生产者输入文件
    for (int i = 0; i < num_producers; ++i) {
        char filename[32];
        snprintf(filename, sizeof(filename), "producer%d.txt", i);
        FILE *fp = fopen(filename, "w");
        for (int j = 0; j < 3; ++j) {
            fprintf(fp, "Data %d from producer %d\n", j + 1, i);
        }
        fclose(fp);
    }

    // 2. 初始化IPC资源
    int shmid = shmget(IPC_KEY, sizeof(struct BufferPool), 0666 | IPC_CREAT);
    struct BufferPool *pool = (struct BufferPool *)shmat(shmid, NULL, 0);
    pool->write_pos = 0;
    pool->read_pos = 0;
    shmdt(pool);

    int semid = semget(IPC_KEY, 3, 0666 | IPC_CREAT);
    union semun su;
    su.val = 1; semctl(semid, SEM_MUTEX, SETVAL, su);
    su.val = BUFFER_SIZE; semctl(semid, SEM_EMPTY, SETVAL, su);
    su.val = 0; semctl(semid, SEM_FULL, SETVAL, su);
    printf("主进程: IPC资源已初始化\n");

    // 3. fork创建子进程
    for (int i = 0; i < num_producers + num_consumers; i++) {
        if (fork() == 0) {
            if (i < num_producers) {
                run_producer(i, shmid, semid);
            } else {
                run_consumer(i - num_producers, shmid, semid);
            }
            exit(0);
        }
    }

    // 4. 父进程等待所有子进程结束
    printf("主进程: 所有子进程已创建，等待它们结束...\n");
    for (int i = 0; i < num_producers + num_consumers; i++) {
        wait(NULL);
    }

    // 5. 清理IPC资源
    printf("主进程: 所有子进程已结束，开始清理IPC资源...\n");
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID, NULL);
    printf("主进程: 清理完成\n");

    return 0;
}
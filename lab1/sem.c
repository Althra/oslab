#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define PHILOSOPHERS 10

sem_t chopsticks[PHILOSOPHERS];
sem_t room; 

void think() {
    sleep(rand()%1 + 0);
}

void eat() {
    sleep(rand()%1 + 0);
}

void* philosopher(void* num) {
    int id = *(int*)num;
    int left = id;
    int right = (id + 1) % PHILOSOPHERS;

    while (1) {
        printf("Philosopher %d is thinking.\n", id);
        think();

        // 请求进入房间，如果房间已满（N-1人），则等待
        sem_wait(&room);
        printf("Philosopher %d entered the room.\n", id);

        // 依次拿起左边和右边的筷子
        sem_wait(&chopsticks[left]);
        printf("Philosopher %d picked up left chopstick %d.\n", id, left);
        sem_wait(&chopsticks[right]);
        printf("Philosopher %d picked up right chopstick %d.\n", id, right);

        printf("Philosopher %d is eating.\n", id);
        eat();

        // 进餐完毕，放下筷子
        sem_post(&chopsticks[right]);
        sem_post(&chopsticks[left]);
        printf("Philosopher %d put down chopsticks.\n", id);

        // 离开房间，为其他等待的哲学家让出位置
        sem_post(&room);
        printf("Philosopher %d left the room.\n", id);
    }
    return NULL;
}

int main() {
    pthread_t threads[PHILOSOPHERS];
    int ids[PHILOSOPHERS];

    // 初始化信号量
    // 'room' 信号量初始值为 PHILOSOPHERS - 1
    sem_init(&room, 0, PHILOSOPHERS - 1); 

    for (int i = 0; i < PHILOSOPHERS; i++) {
        // 每个筷子信号量初始值为1，表示可用
        sem_init(&chopsticks[i], 0, 1); 
        ids[i] = i;
    }

    for (int i = 0; i < PHILOSOPHERS; i++) {
        pthread_create(&threads[i], NULL, philosopher, &ids[i]);
    }

    for (int i = 0; i < PHILOSOPHERS; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&room);
    for (int i = 0; i < PHILOSOPHERS; i++) {
        sem_destroy(&chopsticks[i]);
    }

    return 0;
}
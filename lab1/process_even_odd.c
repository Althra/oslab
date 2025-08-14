#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define PHILOSOPHERS 10

pthread_mutex_t chopsticks[PHILOSOPHERS];

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

        if (id % 2 == 0) {
            // 偶数号哲学家：先拿左筷，再拿右筷
            pthread_mutex_lock(&chopsticks[left]);
            printf("Philosopher %d (even) picked up left chopstick %d.\n", id, left);
            pthread_mutex_lock(&chopsticks[right]);
            printf("Philosopher %d (even) picked up right chopstick %d.\n", id, right);
        } else {
            // 奇数号哲学家：先拿右筷，再拿左筷
            pthread_mutex_lock(&chopsticks[right]);
            printf("Philosopher %d (odd) picked up right chopstick %d.\n", id, right);
            pthread_mutex_lock(&chopsticks[left]);
            printf("Philosopher %d (odd) picked up left chopstick %d.\n", id, left);
        }

        printf("Philosopher %d is eating.\n", id);
        eat();

        pthread_mutex_unlock(&chopsticks[left]);
        pthread_mutex_unlock(&chopsticks[right]);
        printf("Philosopher %d put down chopsticks.\n", id);
    }
    return NULL;
}

int main() {
    pthread_t threads[PHILOSOPHERS];
    int ids[PHILOSOPHERS];

    for (int i = 0; i < PHILOSOPHERS; i++) {
        pthread_mutex_init(&chopsticks[i], NULL);
        ids[i] = i;
    }

    for (int i = 0; i < PHILOSOPHERS; i++) {
        pthread_create(&threads[i], NULL, philosopher, &ids[i]);
    }

    for (int i = 0; i < PHILOSOPHERS; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
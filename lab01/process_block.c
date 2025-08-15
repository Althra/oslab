#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define PHILOSOPHERS 10

pthread_mutex_t mutex[PHILOSOPHERS];

void think() {
    sleep(rand()%1 + 0);
}

void eat() {
    sleep(rand()%1 + 0);
}

void* philosopher(void* num) {
    int id = *(int*)num;
    while (1) {
        printf("Philosopher %d is thinking.\n", id);
        think();

        // Pick up left chopstick
        pthread_mutex_lock(&mutex[id]);
        printf("Philosopher %d picked up left chopstick %d.\n", id, id);

        // Pick up right chopstick
        pthread_mutex_lock(&mutex[(id + 1) % PHILOSOPHERS]);
        printf("Philosopher %d picked up right chopstick %d.\n", id, (id + 1) % PHILOSOPHERS);

        printf("Philosopher %d is eating.\n", id);
        eat();

        pthread_mutex_unlock(&mutex[(id + 1) % PHILOSOPHERS]);
        pthread_mutex_unlock(&mutex[id]);
        printf("Philosopher %d put down chopsticks.\n", id);
    }
    return NULL;
}

int main() {
    pthread_t threads[PHILOSOPHERS];
    int ids[PHILOSOPHERS];

    for (int i = 0; i < PHILOSOPHERS; i++) {
        pthread_mutex_init(&mutex[i], NULL);
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
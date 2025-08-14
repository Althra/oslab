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
    while (1) {
        printf("Philosopher %d is thinking.\n", id);
        think();

        int left = id;
        int right = (id + 1) % PHILOSOPHERS;

        while (1) {
            if (pthread_mutex_trylock(&chopsticks[left]) == 0) {
                printf("Philosopher %d picked up chopstick %d.\n", id, left);

                if (pthread_mutex_trylock(&chopsticks[right]) == 0) {
                    printf("Philosopher %d picked up chopstick %d.\n", id, right);

                    printf("Philosopher %d is eating.\n", id);
                    eat();

                    pthread_mutex_unlock(&chopsticks[right]);
                    pthread_mutex_unlock(&chopsticks[left]);
                    printf("Philosopher %d put down chopsticks.\n", id);
                    break; // Exit retry loop after eating
                } else {
                    pthread_mutex_unlock(&chopsticks[left]);
                    printf("Philosopher %d couldn't pick up chopstick %d, retrying.\n", id, right);
                }
            }
            usleep(100000); // Wait 100ms before retrying
        }
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
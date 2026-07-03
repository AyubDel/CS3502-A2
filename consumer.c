/*
 * Name: Ayub Del
 * Course: CS 3502 Operating Systems
 * Assignment 2: Bounded Buffer
 * File: consumer.c
 */

#include "buffer.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static sem_t *open_existing_sem(const char *name) {
    int tries;
    sem_t *sem;

    for (tries = 0; tries < 50; tries++) {
        sem = sem_open(name, 0);
        if (sem != SEM_FAILED) {
            return sem;
        }
        usleep(100000);
    }

    return SEM_FAILED;
}

static void cleanup_if_last(shared_buffer_t *shared, int shm_id,
                            sem_t *empty, sem_t *full, sem_t *mutex) {
    int should_cleanup = 0;

    if (sem_wait(mutex) == -1) {
        perror("sem_wait mutex during cleanup");
    } else {
        shared->active_processes--;
        if (shared->active_processes <= 0) {
            should_cleanup = 1;
        }
        if (sem_post(mutex) == -1) {
            perror("sem_post mutex during cleanup");
        }
    }

    sem_close(empty);
    sem_close(full);
    sem_close(mutex);
    shmdt(shared);

    if (should_cleanup) {
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);
        shmctl(shm_id, IPC_RMID, NULL);
    }
}

int main(int argc, char *argv[]) {
    int consumer_id;
    int num_items;
    int shm_id;
    shared_buffer_t *shared;
    sem_t *empty;
    sem_t *full;
    sem_t *mutex;
    int i;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <consumer_id> <num_items>\n", argv[0]);
        return EXIT_FAILURE;
    }

    consumer_id = atoi(argv[1]);
    num_items = atoi(argv[2]);

    if (consumer_id <= 0 || num_items < 0) {
        fprintf(stderr, "consumer_id must be positive and num_items must be non-negative\n");
        return EXIT_FAILURE;
    }

    shm_id = shmget(SHM_KEY, sizeof(shared_buffer_t), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }

    shared = (shared_buffer_t *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    empty = open_existing_sem(SEM_EMPTY_NAME);
    full = open_existing_sem(SEM_FULL_NAME);
    mutex = open_existing_sem(SEM_MUTEX_NAME);

    if (empty == SEM_FAILED || full == SEM_FAILED || mutex == SEM_FAILED) {
        fprintf(stderr, "Could not open semaphores. Start a producer first or clean stale IPC resources.\n");
        shmdt(shared);
        return EXIT_FAILURE;
    }

    if (sem_wait(mutex) == -1) {
        perror("sem_wait mutex active count");
        return EXIT_FAILURE;
    }
    shared->active_processes++;
    if (sem_post(mutex) == -1) {
        perror("sem_post mutex active count");
        return EXIT_FAILURE;
    }

    for (i = 0; i < num_items; i++) {
        item_t item;

        if (sem_wait(full) == -1) {
            perror("sem_wait full");
            cleanup_if_last(shared, shm_id, empty, full, mutex);
            return EXIT_FAILURE;
        }
        if (sem_wait(mutex) == -1) {
            perror("sem_wait mutex");
            cleanup_if_last(shared, shm_id, empty, full, mutex);
            return EXIT_FAILURE;
        }

        item = shared->buffer[shared->tail];
        shared->tail = (shared->tail + 1) % BUFFER_SIZE;
        shared->count--;

        printf("Consumer %d: Consumed value %d from Producer %d\n",
               consumer_id, item.value, item.producer_id);
        fflush(stdout);

        if (sem_post(mutex) == -1) {
            perror("sem_post mutex");
            cleanup_if_last(shared, shm_id, empty, full, mutex);
            return EXIT_FAILURE;
        }
        if (sem_post(empty) == -1) {
            perror("sem_post empty");
            cleanup_if_last(shared, shm_id, empty, full, mutex);
            return EXIT_FAILURE;
        }

        usleep(50000);
    }

    cleanup_if_last(shared, shm_id, empty, full, mutex);
    return EXIT_SUCCESS;
}

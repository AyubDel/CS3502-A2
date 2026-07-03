/*
 * Name: Ayub Del
 * Course: CS 3502 Operating Systems
 * Assignment 2: Bounded Buffer
 * File: producer.c
 */

#include "buffer.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    int producer_id;
    int num_items;
    int shm_id;
    int created_segment = 0;
    shared_buffer_t *shared;
    sem_t *empty;
    sem_t *full;
    sem_t *mutex;
    int i;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <producer_id> <num_items>\n", argv[0]);
        return EXIT_FAILURE;
    }

    producer_id = atoi(argv[1]);
    num_items = atoi(argv[2]);

    if (producer_id <= 0 || num_items < 0) {
        fprintf(stderr, "producer_id must be positive and num_items must be non-negative\n");
        return EXIT_FAILURE;
    }

    shm_id = shmget(SHM_KEY, sizeof(shared_buffer_t), IPC_CREAT | IPC_EXCL | 0666);
    if (shm_id == -1) {
        if (errno == EEXIST) {
            shm_id = shmget(SHM_KEY, sizeof(shared_buffer_t), 0666);
            if (shm_id == -1) {
                perror("shmget existing");
                return EXIT_FAILURE;
            }
        } else {
            perror("shmget create");
            return EXIT_FAILURE;
        }
    } else {
        created_segment = 1;
    }

    shared = (shared_buffer_t *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    if (created_segment) {
        sem_unlink(SEM_EMPTY_NAME);
        sem_unlink(SEM_FULL_NAME);
        sem_unlink(SEM_MUTEX_NAME);

        shared->head = 0;
        shared->tail = 0;
        shared->count = 0;
        shared->active_processes = 0;

        empty = sem_open(SEM_EMPTY_NAME, O_CREAT | O_EXCL, 0644, BUFFER_SIZE);
        full = sem_open(SEM_FULL_NAME, O_CREAT | O_EXCL, 0644, 0);
        mutex = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0644, 1);
    } else {
        empty = open_existing_sem(SEM_EMPTY_NAME);
        full = open_existing_sem(SEM_FULL_NAME);
        mutex = open_existing_sem(SEM_MUTEX_NAME);
    }

    if (empty == SEM_FAILED || full == SEM_FAILED || mutex == SEM_FAILED) {
        perror("sem_open");
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

        item.value = producer_id * 1000 + i;
        item.producer_id = producer_id;

        if (sem_wait(empty) == -1) {
            perror("sem_wait empty");
            cleanup_if_last(shared, shm_id, empty, full, mutex);
            return EXIT_FAILURE;
        }
        if (sem_wait(mutex) == -1) {
            perror("sem_wait mutex");
            cleanup_if_last(shared, shm_id, empty, full, mutex);
            return EXIT_FAILURE;
        }

        shared->buffer[shared->head] = item;
        shared->head = (shared->head + 1) % BUFFER_SIZE;
        shared->count++;

        printf("Producer %d: Produced value %d\n", producer_id, item.value);
        fflush(stdout);

        if (sem_post(mutex) == -1) {
            perror("sem_post mutex");
            cleanup_if_last(shared, shm_id, empty, full, mutex);
            return EXIT_FAILURE;
        }
        if (sem_post(full) == -1) {
            perror("sem_post full");
            cleanup_if_last(shared, shm_id, empty, full, mutex);
            return EXIT_FAILURE;
        }

        usleep(50000);
    }

    cleanup_if_last(shared, shm_id, empty, full, mutex);
    return EXIT_SUCCESS;
}

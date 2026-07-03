/*
 * Name: Ayub Del
 * Course: CS 3502 Operating Systems
 * Assignment 2: Bounded Buffer
 * File: buffer.h
 */

#ifndef BUFFER_H
#define BUFFER_H

#define BUFFER_SIZE 10
#define SHM_KEY 0x1234

#define SEM_EMPTY_NAME "/a2_sem_empty_ayub_del"
#define SEM_FULL_NAME  "/a2_sem_full_ayub_del"
#define SEM_MUTEX_NAME "/a2_sem_mutex_ayub_del"

typedef struct {
    int value;
    int producer_id;
} item_t;

typedef struct {
    item_t buffer[BUFFER_SIZE];
    int head;
    int tail;
    int count;
    int active_processes;
} shared_buffer_t;

#endif

//
// Created by drh0use on 3/11/20.
//

#ifndef UNTITLED1_UTILS_H
#define UNTITLED1_UTILS_H

#define TRUE 1
#define FALSE 0
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <semaphore.h>

struct Args
{
    int infd, outfd, port;
    int min_thread, max_thread;
};

/* Client will send two non-negative integers */
struct Packet
{
    unsigned int i1, i2;
};


/* prints usage information and exits. */
void help();

/* prints error */
void xerror(const char *fun, char *msg);

/* malloc with error checking. */
void *xmalloc(size_t size);

/* close with error checking */
void xclose(int fd);

/* open with error checking */
int xopen(const char *file, int oflag);

/* read with error checking */
int xread(int fd, void *buf, size_t size);

/* write with error checking */
int xwrite(int fd, void *buf, size_t size);

/* using get_opt() function, parse arguments to the given 3 params. */
void parse_args(int argc, char **argv, struct Args *args);

/* realloc with error checking */
void *xrealloc(void *old, size_t size);

/* lseek with error checking */
int xlseek(int fd, __off_t off, int flag);

/* pthread_create with error checking */
void xthread_create(pthread_t *thread, void *(*start_routine)(void *), void *arg);

/* pthread_join with error checking */
void xthread_join(pthread_t thread);

/* strtol with error checking */
int str_to_int(char *buf);

/* sem_wait with error checking */
void xsem_wait(sem_t *sem);

/* sem_post with error checking */
void xsem_post(sem_t *sem);

/* sem_init with error checking */
void xsem_init(sem_t *sem, int value);

/* sem_destroy with error checking */
void xsem_destroy(sem_t *sem);

char *timestamp();

#endif //UNTITLED1_UTILS_H

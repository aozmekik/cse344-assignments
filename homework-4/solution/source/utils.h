//
// Created by drh0use on 3/11/20.
//
#ifndef UNTITLED1_UTILS_H
#define UNTITLED1_UTILS_H

#define TRUE 1
#define FALSE 0
// #include <stddef.h>
// #include <sys/stat.h>
// #include <sys/mman.h>
// #include <fcntl.h>
#include <semaphore.h>
#include <sys/types.h>
#include <pthread.h>



/* using get_opt() function, parse arguments to the given 3 params. */
void parse_args(int argc, char **argv, int *fd);

/* prints usage information and exits. */
void help();

/* prints error */
void xerror(const char *fun, char *msg);

/* malloc with error checking. */
void* xmalloc(size_t size);

/* sem_wait with error checking */
void xsem_wait(sem_t *sem);

/* sem_post with error checking */
void xsem_post(sem_t *sem);

/* close with error checking */
void xclose(int fd);

/* sem_init with error checking */
void xsem_init(sem_t *sem, int value);

/* open with error checking */
int xopen(const char* file, int oflag);

/* read with error checking */
int xread(int fd, void* buf, size_t size);

/* sem_destroy with error checking */
void xsem_destroy(sem_t *sem);

/* pthread_create with error checking */
void xthread_create(pthread_t *thread, void *(*start_routine)(void *), void *arg);

/* pthread_join with error checking */
void xthread_join(pthread_t thread);

int systemV_semcreate();
int systemV_seminit(int semid);
void systemV_semwait(int semid, int index[2]);
void systemV_sempost(int semid, int index[2]);
void systemV_semdestroy(int semid);

// /* write with error checking */
// int xwrite(int fd, void* buf, size_t size);

// /* lseek with error checking */
// int xlseek(int fd, __off_t off, int flag);

// /* mkstemp with error checking */
// int xmkstemp(char* template);

// /* strtol with error checking */
// int str2int(char* buf, char **end_ptr);

// /* strtof with error checking */
// float str2float(char* buf, char **end_ptr);

// /* realloc with error checking */
// void* xrealloc(void *old, size_t size);

#endif //UNTITLED1_UTILS_H

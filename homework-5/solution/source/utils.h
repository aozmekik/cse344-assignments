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

/* using get_opt() function, parse arguments to the given 3 params. */
void parse_args(int argc, char **argv, int *fd);

/* realloc with error checking */
void *xrealloc(void *old, size_t size);

/* lseek with error checking */
int xlseek(int fd, __off_t off, int flag);

/* pthread_create with error checking */
void xthread_create(pthread_t *thread, void *(*start_routine)(void *), void *arg);

/* pthread_join with error checking */
void xthread_join(pthread_t thread);

#endif //UNTITLED1_UTILS_H

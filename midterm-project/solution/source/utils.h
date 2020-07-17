//
// Created by drh0use on 3/11/20.
//
#ifndef UNTITLED1_UTILS_H
#define UNTITLED1_UTILS_H

#define TRUE 1
#define FALSE 0
#include <stddef.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

struct Args
{
    // number of;
    int N, // cooks.
        M, // students
        U, // undergraduates.
        G, // graduates.
        T, // tables.
        S, // counter size.
        L, // total rounds by students.
        K; // kitchen size. (not will be input)
    char F[32]; // file path for supplies.
};

/* using get_opt() function, parse arguments to the given 3 params. */
void parse_args(int argc, char **argv, struct Args *args);

/* prints usage information and exits. */
void help();

/* prints error */
void xerror(const char *fun, char *msg);

/* shm_open with error checking */
int xshm_open(char *name, int flag, mode_t mode);

/* ftruncate with error checking */
void xftruncate(int fd, off_t length);

/* close with error checking */
void xclose(int fd);

/* mmap with error checking */
void *xmmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);

/* sem_init with error checking */
void xsem_init(sem_t *sem, int value);


/* malloc with error checking. */
void* xmalloc(size_t size);

/* sem_getvalue with error checking */
void xsem_getvalue(sem_t *sem, int *sval);


/* open with error checking */
int xopen(const char* file, int oflag);

/* read with error checking */
int xread(int fd, void* buf, size_t size);

/* write with error checking */
int xwrite(int fd, void* buf, size_t size);

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

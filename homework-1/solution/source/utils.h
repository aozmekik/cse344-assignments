//
// Created by drh0use on 3/11/20.
//
#include <fcntl.h>
#ifndef UNTITLED1_UTILS_H
#define UNTITLED1_UTILS_H

#define TRUE 1
#define FALSE 0

#define LINE_DELIMETER '\n'
#define EMPTY_DELIMETER 32

#define COMPLEX_BYTE 144

/* using get_opt() function, parse arguments to the given 3 params. */
void parse_arguments(int* in_fd, int* out_fd, int* time, int argc, char** argv, int write);

/* prints usage information and exits. */
void help();

/* malloc with error checking. */
void* xmalloc(size_t size);

/* read with error checking */
int xread(int fd, void* buf, size_t size);

/* write with error checking */
int xwrite(int fd, void* buf, size_t size);

/* open with error checking */
int xopen(const char* file, int oflag);

/* lseek with error checking */
int xlseek(int fd, __off_t off, int flag);

int str2int(char* buff, char **end_ptr);

int is_empty(int line_no, int upper, const int *lines);

int get_empty_line(const int *lines);

int* get_line_list(int fd);

int _get_empty(int fd);

int get_nonempty(int fd);

void delete_line(int fd, int line);

/* locks the file safely */
void lock_file(int fd, struct flock *fl, int read);

/* unlock the file safely */
void unlock_file(int fd, struct flock *fl);

#endif //UNTITLED1_UTILS_H

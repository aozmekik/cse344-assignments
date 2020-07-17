//
// Created by drh0use on 3/11/20.
//
#ifndef UNTITLED1_UTILS_H
#define UNTITLED1_UTILS_H

#define TRUE 1
#define FALSE 0

#define LINE_DELIMETER '\n'
#define EMPTY_DELIMETER 32

#define COMPLEX_BYTE 144

/* using get_opt() function, parse arguments to the given 3 params. */
void parse_args(int argc, char** argv, int* in1_fd, int* in2_fd, int* n);

/* prints usage information and exits. */
void help();

/* open with error checking */
int xopen(const char* file, int oflag);

/* malloc with error checking. */
void* xmalloc(size_t size);

/* read with error checking */
int xread(int fd, void* buf, size_t size);

/* write with error checking */
int xwrite(int fd, void* buf, size_t size);

/* lseek with error checking */
int xlseek(int fd, __off_t off, int flag);

/* mkstemp with error checking */
int xmkstemp(char* template);

/* strtol with error checking */
int str2int(char* buf, char **end_ptr);

/* strtof with error checking */
float str2float(char* buf, char **end_ptr);

/* realloc with error checking */
void* xrealloc(void *old, size_t size);

/* prints error */
void xerror(const char * fun, char* msg);

int compute_svd(float **a, int m, int n, float *w, float **v);

#endif //UNTITLED1_UTILS_H

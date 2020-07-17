/**
 * contains the utility code of program.c, such as parsing args and
 * some error handling routines.
 **/

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "utils.h"
#include <getopt.h>

#define ARGI 'i'

void check_arg(int flag, char arg);

/* parse command line args to given i/o file descriptors */
void parse_args(int argc, char **argv, int *fd)
{
    int flag = 0;

    char opt;
    while ((opt = getopt(argc, argv, "i:")) != -1)
    {
        switch (opt)
        {
        case ARGI:
            printf("Florist application initializing from file: %s", optarg);
            *fd = xopen(optarg, O_RDONLY);
            flag = TRUE;
            break;
        case '?':
        default:
            help();
            exit(EXIT_SUCCESS);
            break;
        }
    }
    check_arg(flag, ARGI);
}

void check_arg(int flag, char arg)
{
    if (!flag)
    {
        fprintf(stderr, "Missing -%c option\n\n", arg);
        help();
        exit(EXIT_FAILURE);
    }
}

/* prints usage */
void help()
{
    printf("Usage: ./program -i [filePath]\n"
           "Example: $./program -i filePath\n"
           "Further information.\n"
           "[filepath] is an absolute/relative file path.\n\n"
           "\t-i:\t\tfile containing data\n"
           "\t--help:\t\tdisplay what you are reading now\n\n"
           "Exis status:\n"
           "0\tif OK,\n"
           "1\tif minor problems (e.g., cannot find the file.), \n"
           "2\tif serious problems.\n\n\n");
}

/* standart functions encapculating functions with error handling */

void xthread_create(pthread_t *thread, void *(*start_routine)(void *), void *arg)
{
    
    int error = pthread_create(thread, NULL, start_routine, arg);
    if (error != 0)
    {
        fprintf(stderr, "Failed to create a thread: %s\n", strerror(error));
        exit(EXIT_SUCCESS);
    }
}

void xthread_join(pthread_t thread)
{
    int error = 0;
    if ((error = pthread_join(thread, NULL)))
    {
        fprintf(stderr, "Failed to join a thread: %s\n", strerror(error));
        exit(EXIT_SUCCESS);
    }
}

void xclose(int fd)
{
    if (close(fd) == -1)
        xerror(__func__, "close");
}

void *xmalloc(size_t size)
{
    void *p;
    if ((p = malloc(size)) == NULL)
    {
        fprintf(stderr, "\nERROR:Out of memory.");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xrealloc(void *old, size_t size)
{
    void *p;
    if ((p = realloc(old, size)) == NULL)
    {
        fprintf(stderr, "\nERROR:Out of memory.");
        exit(EXIT_FAILURE);
    }
    return p;
}

int xread(int fd, void *buf, size_t size)
{
    int read_bytes = read(fd, buf, size);
    if (read_bytes == -1)
    {
        fprintf(stderr, "\nERROR:%s: read input file. errno: %d\n", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return read_bytes;
}

int xopen(const char *file, int oflag)
{
    int fd = 0;
    if ((fd = open(file, oflag, 0666)) < 0)
    {
        fprintf(stderr, "\nERROR: %s: open input file %s. err no: %d\n", __func__, file, errno);
        exit(EXIT_FAILURE);
    }

    return fd;
}

int xlseek(int fd, __off_t off, int flag)
{
    int offset = 0;
    if ((offset = lseek(fd, off, flag)) == -1)
    {
        fprintf(stderr, "\nERROR: %s: lseek error. Err no: %d\n", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return offset;
}

void xerror(const char *fun, char *msg)
{
    fprintf(stderr, "\nError in %s: %s [errno: %d] -> ", fun, msg, errno);
    perror("{ Message } ");
    exit(EXIT_FAILURE);
}

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

void check_arg(int flag, char arg);
int str_to_int(char *buf);

/* parse command line args to given i/o file descriptors */
void parse_args(int argc, char **argv, struct Args *args)
{
    int iflag = FALSE, pflag = FALSE, oflag = FALSE, xflag = FALSE, sflag = FALSE;

    char opt;
    while ((opt = getopt(argc, argv, "i:o:p:s:x:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            args->infd = xopen(optarg, O_RDONLY);
            iflag = TRUE;
            break;
        case 'o':
            args->outfd = xopen(optarg, O_CREAT | O_WRONLY | O_EXCL); 
            oflag = TRUE;
            break;
        case 'p':
            args->port = str_to_int(optarg);
            pflag = TRUE;
            if (args->port < 0 || args->port > 65535)
            {
                fprintf(stderr, "Port arg, is not in range [0, 65535].");
                exit(EXIT_FAILURE);
            }
            break;
        case 's':
            args->min_thread = str_to_int(optarg);
            sflag = TRUE;
            if (args->min_thread < 0)
            {
                fprintf(stderr, "Min number of threads (s) arg, is not in range [0, +inf].");
                exit(EXIT_FAILURE);
            }
            break;
        case 'x':
            args->max_thread = str_to_int(optarg);
            xflag = TRUE;
            if (args->max_thread < 0)
            {
                fprintf(stderr, "Max number of threads (x) arg, is not in range [0, +inf].");
                exit(EXIT_FAILURE);
            }
            break;
        case '?':
        default:
            help();
            exit(EXIT_SUCCESS);
            break;
        }
    }
    check_arg(iflag, 'i');
    check_arg(pflag, 'p');
    check_arg(oflag, 'o');
    check_arg(sflag, 's');
    check_arg(xflag, 'x');
    dprintf(args->outfd, "[%s] Executing with parameters: \n", timestamp());
    dprintf(args->outfd, "-i %s\n-p %s\n-o %s\n-s %s\n-x %s\n", 
    argv[2], argv[4], argv[6], argv[8], argv[10]);
    if (args->max_thread < args->min_thread)
        xerror(__func__, "error: max thread count < min thread count");
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
        if (errno == EEXIST)
        {
            remove(file);
            return xopen(file, oflag);
        }
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

int str_to_int(char *buf)
{
    errno = 0;
    char *end_ptr;
    int num = (int)strtol(buf, &end_ptr, 10);

    // possible strtol errors.
    if ((errno == ERANGE && (num == INT_MAX || num == INT_MIN)) || (errno != 0 && num == 0))
    {
        fprintf(stderr, "Error strtol: %s, num %d, ERRNO:%d", buf, num, errno);

        exit(EXIT_FAILURE);
    }
    if (end_ptr == buf)
    {
        fprintf(stderr, "No digits were found in time arg.\n");
        exit(EXIT_FAILURE);
    }

    return num;
}

int xwrite(int fd, void *buf, size_t size)
{
    int write_byte = write(fd, buf, size);
    if (write_byte == -1)
    {
        fprintf(stderr, "\nERROR: %s: write output file. err no: %d\n", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return write_byte;
}

void xsem_wait(sem_t *sem)
{
    if (sem_wait(sem) == -1)
        xerror(__func__, "sem_wait");
}

void xsem_post(sem_t *sem)
{
    if (sem_post(sem) == -1)
        xerror(__func__, "sem_post");
}

void xsem_init(sem_t *sem, int value)
{
    if (sem_init(sem, FALSE, (unsigned int)value) == -1)
        xerror(__func__, "sem_init");
}

void xsem_destroy(sem_t *sem)
{
    if (sem_destroy(sem) == -1)
        xerror(__func__, "sem_destroy");
}


char *timestamp()
{
    time_t now;
    now = time(NULL);
    char *time_str = ctime(&now);
    char *new_line = strchr(time_str, '\n');
    new_line[0] = '\0';
    return time_str;
}

/**
 * contains the utility code of program.c, such as parsing args and
 * some error handling routines.
 **/
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "utils.h"
#include <getopt.h>
#include <assert.h>
#include <sys/mman.h>
#include <semaphore.h>

#define ARGN 'N'
#define ARGU 'U'
#define ARGG 'G'
#define ARGT 'T'
#define ARGS 'S'
#define ARGL 'L'
#define ARGF 'F'
#define ARGM 'M'

void check_arg(int flag, char arg);
void check_range(int num, char arg, int min);
int str2int(char *buf);

/* parse command line args to given i/o file descriptors */
void parse_args(int argc, char **argv, struct Args *args)
{
    struct Flags
    {
        int N, U, G, T, S, L, F;
    } flags;

    int opt = 0; // option for arguments.
    memset(&flags, 0, sizeof(struct Flags));
    memset(args, 0, sizeof(struct Args));

    while ((opt = getopt(argc, argv, "N:T:U:G:L:S:F:")) != -1)
    {
        switch (opt)
        {
        case ARGN:
            args->N = str2int(optarg);
            flags.N = TRUE;
            check_range(args->N, ARGN, 3); // N > 2
            break;
        case ARGU:
            args->U = str2int(optarg);
            flags.U = TRUE; // we check the range afterwards.
            break;
        case ARGG:
            args->G = str2int(optarg);
            flags.G = TRUE;
            check_range(args->G, ARGG, 1); // G >= 1
            break;
        case ARGT:
            args->T = str2int(optarg);
            flags.T = TRUE;
            check_range(args->T, ARGT, 2); // T > 1
            break;
        case ARGS:
            args->S = str2int(optarg);
            flags.S = TRUE;
            check_range(args->S, ARGS, 4); // S > 3
            break;
        case ARGL:
            args->L = str2int(optarg);
            flags.L = TRUE;
            check_range(args->L, ARGL, 3); // L >= 3
            break;
        case ARGF:
            strcpy(args->F, optarg);
            flags.F = TRUE;
            break;
        case '?':
        default:
            help();
            exit(EXIT_SUCCESS);
            break;
        }
    }
    args->M = args->U + args->G;
    args->K = (2 * args->L * args->M) + 1;
    check_arg(flags.N, ARGN);
    check_arg(flags.U, ARGU);
    check_arg(flags.G, ARGG);
    check_arg(flags.T, ARGT);
    check_arg(flags.S, ARGS);
    check_arg(flags.L, ARGL);
    check_arg(flags.F, ARGF);
    check_range(args->U, ARGU, args->G); // U > G
    check_range(args->M, ARGM, args->N > args->T ? args->N + 1 : args->T + 1); // M > N, M > T
    check_range(args->M, ARGM, 3); // M > 3
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

void check_range(int num, char arg, int min)
{
    if (num < min || num > INT_MAX)
    {
        char errmsg[64];
        sprintf(errmsg, "%c: %d is not in range [%d, +inf]\n", arg, num, min);
        xerror("parse_args", errmsg);
    }
}

/* prints usage */
void help()
{
    printf("Usage: ./program -N [#cooks] -M [#students] -T [#tables] -S [#counter] -L [#times] -F [filePath]\n"
           "Example: $./program -N 3 -T 5 -S 4 -L 13 -U 8 -G 2 -F filePath\n"
           "Further information.\n"
           "[filepath] is an absolute/relative file path.\n\n"
           "\t-N:\t\tnumber of cooks.\n"
           "\t-U:\t\tnumber of undergraduate students.\n"
           "\t-G:\t\tnumber of graduate students.\n"
           "\t-T:\t\tnumber of tables.\n"
           "\t-S:\t\tcounter size.\n"
           "\t-L:\t\tnumber of times students will eat.\n"
           "\t--help:\t\tdisplay what you are reading now\n\n"
           "Exis status:\n"
           "0\tif OK,\n"
           "1\tif minor problems (e.g., cannot find the file.), \n"
           "2\tif serious problems.\n\n\n");
}

/* standart functions encapculating functions with error handling */

int xshm_open(char *name, int flag, mode_t mode)
{
    int fd = shm_open(name, flag, mode);
    if (fd == -1)
    {
        if (errno == EEXIST)
        {
            if (shm_unlink(name) == -1)
                xerror(__func__, "shm_unlink");
            return xshm_open(name, flag, mode);
        }
        else 
            xerror(__func__, "shm_open");
    }
    return fd;
}

void xsem_init(sem_t *sem, int value)
{
    if (sem_init(sem, TRUE, (unsigned int) value) == -1)
        xerror(__func__, "sem_init");
}

void xsem_getvalue(sem_t *sem, int *sval)
{
    if (sem_getvalue(sem, sval) == -1)
        xerror(__func__, "sem_getvalue");
}

void xftruncate(int fd, off_t length)
{
    if (ftruncate(fd, length) == -1)
        xerror(__func__, "ftruncate");
}

void *xmmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    void *p = mmap(addr, len, prot, flags, fd, offset);
    if (p == MAP_FAILED)
        xerror(__func__, "mmap");
    return p;
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

int xmkstemp(char *template)
{
    int fd = 0;
    if ((fd = mkstemp(template)) == -1)
    {
        fprintf(stderr, "\nERROR:%s: mkstemp error. errno : %d\n", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return fd;
}

int str2int(char *buf)
{
    errno = 0;
    char *end_ptr = NULL;
    int num = (int)strtol(buf, &end_ptr, 10);

    // possible strtol errors.
    if ((errno == ERANGE && (num == INT_MAX || num == INT_MIN)) || (errno != 0 && num == 0))
    {
        fprintf(stderr, "Error strtol: %s, num %d, ERRNO:%d", buf, num, errno);

        exit(EXIT_FAILURE);
    }
    if (end_ptr == buf)
    {
        fprintf(stderr, "No digits were found in the arg.\n");
        exit(EXIT_FAILURE);
    }

    return num;
}

float str2float(char *buf, char **end_ptr)
{
    errno = 0;
    float num = strtof(buf, end_ptr);

    // possible strtol errors.
    if ((errno == ERANGE && (num == HUGE_VALF || num == HUGE_VALL)) || (errno != 0 && num == 0))
    {
        fprintf(stderr, "Error strtod: %s, num %f, ERRNO:%d", buf, num, errno);

        exit(EXIT_FAILURE);
    }
    if (*end_ptr == buf)
    {
        fprintf(stderr, "No digits were found in time arg.\n");
        exit(EXIT_FAILURE);
    }

    return num;
}

void xerror(const char *fun, char *msg)
{
    fprintf(stderr, "Error in %s: %s [errno: %d] -> ", fun, msg, errno);
    perror("{ Message } ");
    exit(EXIT_FAILURE);
}

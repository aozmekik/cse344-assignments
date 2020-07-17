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
#include <sys/sem.h>

union semun {
    int val;               /* semaphore value */
    struct semid_ds *buf;  /* semaphore data structure */
    unsigned short *array; /* multiple semaphore values */
    struct seminfo *__buf; /* linux specific */
};

#define ARGI 'i'

void check_arg(int flag, char arg);
void check_range(int num, char arg, int min);
int str2int(char *buf);

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
    printf("Usage: ./program -i [filePath]\n"
           "Example: $./program -i filePath\n"
           "Further information.\n"
           "[filepath] is an absolute/relative file path.\n\n"
           "\t-i:\t\tfile containing supplies\n"
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

int systemV_semcreate()
{
    int id = 0;
    id = semget(IPC_PRIVATE, 4, 0666 | IPC_EXCL| O_CREAT);
    if (id == -1)
        xerror(__func__, "semget");
    return id;
}

int systemV_seminit(int semid)
{
    union semun arg;
    unsigned short values[4];
    for (int i = 0; i < 4; i++)
        values[i] = 0;
    arg.array = values;
    int ret = semctl(semid, 0, SETALL, arg);
    if (ret == -1){
        systemV_semdestroy(semid);
        xerror(__func__, "semctl");
    }
    return ret;
}

void systemV_semwait(int semid, int index[2])
{
    struct sembuf ops[2];
    ops[0].sem_num = index[0];
    ops[1].sem_num = index[1];
    ops[0].sem_op = ops[1].sem_op = -1;
    ops[0].sem_flg = ops[1].sem_flg = 0;
    if (semop(semid, ops, 2) == -1)
    {
        if (errno != EINTR)
            xerror(__func__, "semop");
        else
            systemV_semwait(semid, index); // try again.
    }
}

void systemV_sempost(int semid, int index[2])
{
    struct sembuf ops[2];
    ops[0].sem_num = index[0];
    ops[1].sem_num = index[1];
    ops[0].sem_op = ops[1].sem_op = 1;
    ops[0].sem_flg = ops[1].sem_flg = 0;
    if (semop(semid, ops, 2) == -1)
        xerror(__func__, "semop");

}

void systemV_semdestroy(int semid)
{
    if (semctl(semid, 0, IPC_RMID))
        xerror(__func__, "seminit");
}

int xsem_trywait(sem_t *sem)
{
    int ret = 0;
    if ((ret = sem_trywait(sem)) == -1 && errno != EAGAIN)
        xerror(__func__, "sem_trywait");
    return ret;
}

void xsem_init(sem_t *sem, int value)
{
    if (sem_init(sem, FALSE, (unsigned int)value) == -1)
        xerror(__func__, "sem_init");
}

void xsem_destroy(sem_t *sem)
{
    if (sem_destroy(sem) == -1)
        xerror(__func__, "sem_destory");
}

// sem wait.
void xsem_wait(sem_t *sem)
{
    if (sem_wait(sem) == -1)
        xerror(__func__, "sem_wait");
}

// sem post.
void xsem_post(sem_t *sem)
{
    if (sem_post(sem) == -1)
        xerror(__func__, "sem_post");
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
    fprintf(stderr, "\nError in %s: %s [errno: %d] -> ", fun, msg, errno);
    perror("{ Message } ");
    exit(EXIT_FAILURE);
}

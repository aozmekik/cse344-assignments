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

#define ARG_INPUT1 'i'
#define ARG_INPUT2 'j'
#define ARG_N 'n'

#define TRUE 1
#define FALSE 0

void check_arg(int flag, char arg);

/* parse command line args to given i/o file descriptors */
void parse_args(int argc, char** argv, int* in1_fd, int* in2_fd, int* n){
    int i1_flag = FALSE, i2_flag = FALSE, n_flag = FALSE; // flags for -i, -o.
    int opt = 0; // option for arguments.

    while ((opt = getopt(argc, argv, "i:j:n:")) != -1)
    {
        switch (opt)
        {
            case ARG_INPUT1:
                i1_flag = TRUE;
                *in1_fd = xopen(optarg, O_RDONLY);
                break;
            
            case ARG_INPUT2:
                i2_flag = TRUE;
                *in2_fd = xopen(optarg, O_RDONLY); 
                break;

            case ARG_N:
                n_flag = TRUE;
                char* end_ptr;
                *n = str2int(optarg, &end_ptr);
                if (*n < 0 || *n > INT_MAX) {
                    fprintf(stderr, "Time arg, is not in range [0,+inf].");
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
    
    check_arg(i1_flag, ARG_INPUT1);
    check_arg(i2_flag, ARG_INPUT2);
    check_arg(n_flag, ARG_N);


}

void check_arg(int flag, char arg){
    if (!flag){
        fprintf(stderr, "Missing -%c option\n\n", arg);
        help();
        exit(EXIT_FAILURE);
    }
}


/* prints usage */
void help(){
    printf("Usage: ./programA -i [FILE] -o [FILE]\n"
           "Example: ./programA -i \"input_file.txt\" -o \"output_file.txt\" \n"
           "[FILE] is an absolute file path.\n\n"
           "Further information.\n"
           "\t-i:\t\tfirst input file.\n"
           "\t-j:\t\tsecond input file.\n"
           "\t--help:\t\t display what you are reading now\n\n"
           "Exis status:\n"
           "0\tif OK,\n"
           "1\tif minor problems (e.g., cannot find the file.), \n"
           "2\tif serious problems.\n\n\n" );
}

/* standart functions encapculating functions with error handling */


void* xmalloc(size_t size){
    void* p;
    if ((p = malloc(size)) == NULL){
        fprintf(stderr, "\nERROR:Out of memory.");
        exit(EXIT_FAILURE);
    }
    return p;
}

void* xrealloc(void *old, size_t size){
    void* p;
    if ((p = realloc(old, size)) == NULL){
        fprintf(stderr, "\nERROR:Out of memory.");
        exit(EXIT_FAILURE);
    }
    return p;
}

int xread(int fd, void* buf, size_t size){
    int read_bytes = read(fd, buf, size);
    if(read_bytes == -1){
        fprintf(stderr, "\nERROR:%s: read input file. errno: %d\n", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return read_bytes;
}

int xopen(const char* file, int oflag){
    int fd = 0;
    if ((fd = open(file, oflag, 0666)) < 0) {
        fprintf(stderr, "\nERROR: %s: open input file %s. err no: %d\n", __func__, file, errno);
        exit(EXIT_FAILURE);
    }

    return fd;
}


int xwrite(int fd, void* buf, size_t size){
    int write_byte = write(fd, buf, size);
    if(write_byte == -1){
        fprintf(stderr, "\nERROR: %s: write output file. err no: %d\n", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return write_byte;
}

int xlseek(int fd, __off_t off, int flag){
    int offset = 0;
    if ((offset = lseek(fd, off, flag)) == -1){
        fprintf(stderr, "\nERROR: %s: lseek error. Err no: %d\n", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return offset;
}

int xmkstemp(char* template){
    int fd = 0;
    if ((fd = mkstemp(template)) == -1){
        fprintf(stderr, "\nERROR:%s: mkstemp error. errno : %d\n", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return fd;
}


int str2int(char* buf, char **end_ptr){
    errno = 0;
    int num = (int) strtol(buf, end_ptr, 10);

    // possible strtol errors.
    if ((errno == ERANGE && (num == INT_MAX || num == INT_MIN))
        || (errno != 0 && num == 0)) {
        fprintf(stderr, "Error strtol: %s, num %d, ERRNO:%d", buf, num, errno);

        exit(EXIT_FAILURE);
    }
    if (*end_ptr == buf) {
        fprintf(stderr, "No digits were found in the arg.\n");
        exit(EXIT_FAILURE);
    }

    return num;
}

float str2float(char* buf, char **end_ptr){
    errno = 0;
    float num = strtof(buf, end_ptr);

    // possible strtol errors.
    if ((errno == ERANGE && (num == HUGE_VALF || num == HUGE_VALL))
        || (errno != 0 && num == 0)) {
        fprintf(stderr, "Error strtod: %s, num %f, ERRNO:%d", buf, num, errno);

        exit(EXIT_FAILURE);
    }
    if (*end_ptr == buf) {
        fprintf(stderr, "No digits were found in time arg.\n");
        exit(EXIT_FAILURE);
    }

    return num;
}


void xerror(const char* fun, char* msg){
    fprintf(stderr, "Error in %s: %s, [errno: %d.] Exiting...", fun, msg, errno);
    exit(EXIT_FAILURE);
}

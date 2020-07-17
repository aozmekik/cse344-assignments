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
#include <limits.h>
#include "utils.h"

#define ARG_INPUT 'i'
#define ARG_OUTPUT 'o'

#define TRUE 1
#define FALSE 0

void check_arg(int flag, char arg);

/* parse command line args to given i/o file descriptors */
void parse_args(int argc, char** argv, int* in_fd, int* out_fd){
    int opt = 0; // option for arguments.
    int iflag = 0, oflag = 0; // flags for -i, -o.


    while ((opt = getopt(argc, argv, "i:o:t:")) != -1)
    {
        switch (opt)
        {
            case ARG_INPUT:
                iflag = TRUE;
                *in_fd = xopen(optarg, O_RDWR);
                break;
            
            case ARG_OUTPUT:
                oflag = TRUE;
                *out_fd = xopen(optarg, O_CREAT | O_RDWR); // permission read and write.
                break;
    
            case '?':
            default:
                help();
                exit(EXIT_SUCCESS);
                break;
        }
    }
    
    check_arg(iflag, ARG_INPUT);
    check_arg(oflag, ARG_OUTPUT);
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
           "\t-i:\t\tread bytes from given file\n"
           "\t-o:\t\twrite complex numbers to given file\n"
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
        fprintf(stderr, "No digits were found in time arg.\n");
        exit(EXIT_FAILURE);
    }

    return num;
}

void xerror(const char* fun){
    fprintf(stderr, "Error in %s. ERRNO: %d. Exiting...", fun, errno);
    exit(EXIT_FAILURE);
}

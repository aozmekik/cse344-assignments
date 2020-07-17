//
// Created by drh0use on 3/11/20.
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#include "utils.h"

#define TIME_MAX 50
#define TIME_MIN 1

#define ARG_INPUT 'i'
#define ARG_OUTPUT 'o'
#define ARG_TIME 't'


void check_arg_given(int flag, char arg);

void parse_arguments(int* in_fd, int* out_fd, int* time, int argc, char** argv, int write) {
    int opt; // option for arguments.
    int iflag = 0, oflag = 0, tflag = 0; // flags for -i, -o and -t.

    while ((opt = getopt(argc, argv, "i:o:t:")) != -1)
        switch (opt) {
            case ARG_INPUT:
                iflag = 1;
                if (write)
                    *in_fd = xopen(optarg, O_RDWR);
                else
                    *in_fd = xopen(optarg, O_RDONLY);
                break;
            case ARG_OUTPUT:
                oflag = 1;
                *out_fd = xopen(optarg, O_CREAT | O_RDWR);
                break;
            case ARG_TIME:
                tflag = 1;
                char *end_ptr;
                *time = str2int(optarg, &end_ptr);

                // out of boundaries.
                if (*time < TIME_MIN || *time > TIME_MAX) {
                    fprintf(stderr, "Time arg, is not in range [1,50].");
                    exit(EXIT_FAILURE);
                }
                break;
            case '?':
            default:
                help();
                exit(1);
        }
    /* check three args */
    check_arg_given(iflag, ARG_INPUT);
    check_arg_given(oflag, ARG_OUTPUT);
    check_arg_given(tflag, ARG_TIME);

}

int str2int(char* buff, char **end_ptr){
    int num = (int) strtol(buff, end_ptr, 10);

    // possible strtol errors.
    if ((errno == ERANGE && (num == INT_MAX || num == INT_MIN))
        || (errno != 0 && num == 0)) {
        fprintf(stderr, "strtol");
        exit(EXIT_FAILURE);
    }
    if (*end_ptr == optarg) {
        fprintf(stderr, "No digits were found in time arg.\n");
        exit(EXIT_FAILURE);
    }

    return num;
}

void help(){
    printf("Usage: ./programA -i [FILE] -o [FILE] -t [NUMBER]\n"
           "What is this program is used for [WRITE SOMETHING HERE.]\n"
           "Example: ./programA -i \"input_file.txt\" -o \"output_file.txt\" -t 20 \n"
           "[FILE] is an absolute file path and [NUMBER] is an integer in [1, 50].\n\n"
           "Further information.\n"
           "\t-i:\t\tread bytes from given file\n"
           "\t-o:\t\twrite complex numbers to given file\n"
           "\t-t:\t\tspecify time by giving number in [1, 50]\n"
           "\t--help:\t\t display what you are reading now\n\n"
           "Exis status:\n"
           "0\tif OK,\n"
           "1\tif minor problems (e.g., cannot find the file or number is not in range.), \n"
           "2\tif serious problems (e.g., [WRITE SOMETHING HERE.]).\n\n\n" );
}


void* xmalloc(size_t size){
    void* p;
    if ((p = malloc(size)) == NULL){
        fprintf(stderr, "Out of memory.\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

int xread(int fd, void* buf, size_t size){
    int read_bytes = read(fd, buf, size);
    if(read_bytes == -1){
        fprintf(stderr, "read input file");
        exit(EXIT_FAILURE);
    }
    return read_bytes;
}

/* open with error checking */
int xopen(const char* file, int oflag){
    int fd = 0;
    if ((fd = open(file, oflag)) < 0) {
        fprintf(stderr, "open input file");
        exit(EXIT_FAILURE);
    }

    return fd;
}

int xwrite(int fd, void* buf, size_t size){
    int write_byte = write(fd, buf, size);
    if(write_byte == -1){
        fprintf(stderr, "%s: write output file", __func__);
        fprintf(stderr, "Errno: %d", errno);
        exit(EXIT_FAILURE);
    }
    return write_byte;
}

int xlseek(int fd, __off_t off, int flag){
    int offset = 0;
    if ((offset = lseek(fd, off, flag)) == -1){
        fprintf(stderr, "%s: lseek error. Err no %d", __func__, errno);
        exit(EXIT_FAILURE);
    }
    return offset;
}

void check_arg_given(int flag, char arg){
    if (flag == 0){
        fprintf(stderr, "Missing -%c option\n\n", arg);
        help();
        exit(EXIT_FAILURE);
    }
}

void delete_line(int fd, int line){
    assert(line >= 0);
    xlseek(fd, line * COMPLEX_BYTE, SEEK_SET); // refresh offset.

    char buf[COMPLEX_BYTE]; // overwrite for delete.
    for (int i = 0; i < COMPLEX_BYTE; ++i)
        buf[i] = EMPTY_DELIMETER;
    buf[0] = '\n';

    xwrite(fd, buf, COMPLEX_BYTE);
}


int get_empty_line(const int *lines){
    int upper=0; // upper limit of rand num.
    while (lines[upper] != -1) ++upper;

    if (upper == 0) // there is only one line.
        return -1;

    int line_no = 0;

    // check if it is empty line.
    while (!is_empty(line_no, upper, lines)){
        ++line_no; // search for a new line linearly.
        if (line_no > upper)
            line_no = 0;
    }

    if (line_no != upper-1)
        --line_no;
    return line_no;
}

int is_empty(int line_no, int upper, const int *lines){
    if (line_no + 1 == upper)
        return TRUE; // it is empty, since it is the last line.
    if (line_no + 1 != upper && lines[line_no] - lines[line_no-1] == 1)
        return TRUE; // two '\n' is side by side, it is empty.

    return FALSE;
}

int* get_line_list(int fd) {
    int char_limit = 1024;
    char buf[char_limit];

    int line_limit = 32, line_number = 0;
    int *lines = (int *) xmalloc(sizeof(int) * line_limit); // position of the lines.

    xlseek(fd, 0, SEEK_SET); // set offset.

    int read_bytes;
    int run = 0; // first run.
    while ((read_bytes = xread(fd, buf, char_limit)) > 0) {
        for (int i = 0; i < read_bytes; ++i) {
            if (buf[i] == LINE_DELIMETER)
                lines[line_number++] = i + (char_limit * run);
        }
        ++run;
    }

    lines[line_number] = -1; // end of array indicator.
    return lines;
}

int _get_empty(int fd) {
    int char_limit = 1024;
    char buf[char_limit];


    xlseek(fd, 0, SEEK_SET); // set offset.

    int read_bytes, bytes = 0;
    int run = 0; // first run.
    while ((read_bytes = xread(fd, buf, char_limit)) > 0) {
        bytes = read_bytes + (char_limit*run);
        for (int i = 0; i < read_bytes; ++i) {
            if (buf[i] == LINE_DELIMETER && buf[i+1] == EMPTY_DELIMETER) // get the first empty.
                return i + (char_limit * run);
        }
        ++run;
    }

    return bytes;
}

int get_nonempty(int fd) {
    int char_limit = 1024;
    char buf[char_limit];


    xlseek(fd, 0, SEEK_SET); // set offset.

    int read_bytes, bytes = 0, pos=0;
    int run = 0; // first run.
    while ((read_bytes = xread(fd, buf, char_limit)) > 0) {
        for (int i = 0; i < read_bytes; ++i) {
            pos = i + (char_limit * run);
            if (pos == 0 && buf[i] != EMPTY_DELIMETER && buf[i] != LINE_DELIMETER)
                return pos;
            if (buf[i] == LINE_DELIMETER && buf[i+1] != EMPTY_DELIMETER) // get the first non empty.
                return pos;
            if (pos == read_bytes - COMPLEX_BYTE && buf[i-1] == EMPTY_DELIMETER && buf[i] != LINE_DELIMETER)
                return pos;
        }
        ++run;
    }

    return -1;
}

void lock_file(int fd, struct flock *fl, int read){
    if (read)
        fl->l_type = F_RDLCK;
    else
        fl->l_type = F_WRLCK;

    if (fcntl(fd, F_SETLKW, fl) == -1) {
        fprintf(stderr, "%s: fcntl", __func__);
        exit(EXIT_FAILURE);
    }
}

void unlock_file(int fd, struct flock *fl){
    fl->l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, fl) == -1) {
        fprintf(stderr, "%s: fcntl", __func__);
        exit(EXIT_FAILURE);
    }
}
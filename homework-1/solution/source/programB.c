//
// Created by drh0use on 3/11/20.
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#include "utils.h"
#include "fft.h"


#define NUMBER_LIMIT 16

#define STOP -1


/* gets the position of nth new line from the file
 * usage: readline(0) gives the position of first line.  */
int* get_line_list(int fd);

int get_nonempty_line(int random, const int *lines);

int _read(int fd, char* buf);

void _write(int fd, char* buf, int byte);

int is_empty(int line_no, int upper, const int *lines);

complex _str2complex(char** str);

void buf2complex(complex* nums, char *buf);

int complex2buf(complex* nums, char* buf);

int fft_16complex(char *buf);

int get_empty_line(const int *lines);

int read_16complex(int fd, int line, char* buf);



int main(int argc, char** argv){

    int in_fd=0, out_fd=0, time=0;

    // parse args.
    parse_arguments(&in_fd, &out_fd, &time, argc, argv, 1);

    struct flock fl = {F_RDLCK, SEEK_SET, 0,0,0};
    fl.l_pid = getpid();

    char buf[NUMBER_LIMIT*14];

    int bytes = 0;
    do{
        lock_file(in_fd, &fl, TRUE);
        bytes = _read(in_fd, buf);
        unlock_file(in_fd, &fl);
        if(bytes != STOP){
            lock_file(out_fd, &fl, FALSE);
            _write(out_fd, buf, bytes);
            unlock_file(out_fd, &fl);
            sleep(time);
        }
    } while(bytes != STOP);

    close(in_fd);
    close(out_fd);
    return EXIT_SUCCESS;
}

int _read(int fd, char *buf){
//    int* lines = get_line_list(fd);
//    int line = get_nonempty_line(TRUE, lines);

    int line = get_nonempty(fd);

    if (line == -1)
        return STOP;

    read_16complex(fd, line, buf);
    delete_line(fd, line / 144);

    return fft_16complex(buf);
}

void _write(int fd, char* buf, int byte){
    xlseek(fd, _get_empty(fd), SEEK_SET);
    xwrite(fd, buf, byte);
}


int fft_16complex(char *buf){
    complex nums[NUMBER_LIMIT], temp[NUMBER_LIMIT];
    buf2complex(nums, buf);

    // find fft.
    calc_fft(nums, NUMBER_LIMIT, temp);
    int write_byte = complex2buf(nums, buf);
    return write_byte;
}

void buf2complex(complex* nums, char *buf){
    for (int i = 0; i < NUMBER_LIMIT; ++i) {
        nums[i] = _str2complex(&buf);
    }
}

int complex2buf(complex* nums, char* buf){
    char* out_offset = buf;
    char* tmp = (char*) xmalloc(9);

    int write_byte = 0;
    for (int i = 0; i < NUMBER_LIMIT; ++i) {
        if (nums[i].y < 0)
            sprintf(tmp, "%d-i%d,", (int)nums[i].x, -(int)nums[i].y);
        else
            sprintf(tmp, "%d+i%d,", (int)nums[i].x, (int)nums[i].y);
        sprintf(out_offset, "%s", tmp);
        write_byte += (int) strlen(tmp);
        out_offset += strlen(tmp);
    }
    free(tmp);

    // add a new line to the buffer.
    int j=0;
    for (; j < COMPLEX_BYTE - write_byte - 1; ++j) {
        out_offset[j] = EMPTY_DELIMETER;
    }

    out_offset[j] = '\n';
    write_byte = 144;
    return write_byte;
}

complex _str2complex(char** str){
    complex num;

    char *end_ptr;
    num.x = str2int(*str, &end_ptr);
    assert(*end_ptr == '+' || *end_ptr == '-');
    *str = end_ptr + 2; // pass '+'.

    num.y = str2int(*str, &end_ptr);
    assert(*end_ptr == ',');

    *str = end_ptr + 1; // pass ','.
    return num;
}

int read_16complex(int fd, int line, char* buf) {
    xlseek(fd, line, SEEK_SET); // set offset.
    return xread(fd, buf, COMPLEX_BYTE);
}


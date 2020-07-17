//
// Created by drh0use on 3/10/20.
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

/* 32 bytes will be read in each turn, therefore 32/2 bytes will be written each time */
#define READ_LIMIT 32
#define WRITE_LIMIT 16

#define STOP 1

/* continues reading 32 bytes from the opened file (fd)
 * and converts them to 16 complex numbers.
 * returns: the string consisting 16 comma separated complex numbers.
 * programmer must handle the de allocation of that string.
 */
int read_32byte(int fd, char in_buf[], char out_buf[]);

/* continues writing 16 complex numbers to the opened file (fd) */
void write_16numbers(int fd, char buf[], int write_byte);

/* turns given string to the comma seperated complex numbers
 * returns how many bytes are written . */
int str2complex(char* in_buf, char* out_buf);

int main(int argc, char** argv) {
    int in_fd=0, out_fd=0, time=0;

    // get the args from stdin.
    parse_arguments(&in_fd, &out_fd, &time, argc, argv, 0);

    // out_buf = [%d]*2 (3+3 bytes) + [+, i, ','] (3) = 9 * 16
    char in_buf[READ_LIMIT], out_buf[WRITE_LIMIT*9];

    struct flock fl = {F_RDLCK, SEEK_SET, 0,0,0};
    fl.l_pid = getpid();

    int byte = 0;
    do{
        lock_file(in_fd, &fl, TRUE);
        byte = read_32byte(in_fd, in_buf, out_buf);
        unlock_file(in_fd, &fl);
        if(byte != STOP){
            lock_file(out_fd, &fl, FALSE);
            write_16numbers(out_fd, out_buf, byte);
            unlock_file(out_fd, &fl);
            sleep(time);
        }
    } while(byte != STOP);

    close(in_fd);
    close(out_fd);
    return EXIT_SUCCESS;
}


int read_32byte(int fd, char in_buf[], char out_buf[]){

    int read_bytes = xread(fd, in_buf, READ_LIMIT);

    if(read_bytes < READ_LIMIT) // reached to EOF, or file doesn't contain 32 more bytes.
        return STOP;

    return str2complex(in_buf, out_buf);
}

void write_16numbers(int fd, char buf[], int write_byte){
    // find place and set the file offset to that location, then write to it.
    xlseek(fd, _get_empty(fd), SEEK_SET);
    xwrite(fd, buf, write_byte);
}


/* turns given string to the comma seperated complex numbers
 * returns how many bytes are written . */
int str2complex(char* in_buf, char* out_buf){
    char* out_offset = out_buf;
    char* in_offset = in_buf;
    char* tmp = (char*) xmalloc(9); // check above for magic number 9.

    int write_byte = 0;
    for (int i = 0; i < WRITE_LIMIT; ++i) {
        sprintf(tmp, "%d+i%d,", in_offset[0], in_offset[1]);
        sprintf(out_offset, "%s", tmp);
        write_byte += (int) strlen(tmp);
        out_offset += strlen(tmp);
        in_offset += 2; // 2 bytes read.
    }
    free(tmp);

    // add a new line to the buffer.
    int j=0;
    for (; j < 144 - write_byte - 1; ++j) {
        out_offset[j] = EMPTY_DELIMETER;
    }


    out_offset[j] = '\n';
    write_byte = 144;
    return write_byte;
}




#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <signal.h>
#include <wait.h>
#include <errno.h>
#include "utils.h"

#define BYTES 20 // will be reading 20 bytes at a time. 
#define NEWLINE_DELIMETER '\n'
#define SPACE ' '

struct point {
    float x;
    float y;
};

struct metric {
    float mae, mse, rmse;
};


struct line {
    struct point points[11];
    float mae, mse, rmse;
};


struct metrics_stats {
    struct metric* metrics;
    unsigned int capacity, idx;
};


void P1(int childID);
void P2();
void block_sig(int sig);
void unblock_sig(int sig);

// defined globally to cleanup in case of SIGTERM.
char temp_name[] = "TEMPXXXXXX"; 
int fd_in = 0, fd_out = 0, fd_temp = 0; 


int main(int argc, char* argv[]){
    
    parse_args(argc, argv, &fd_in, &fd_out);
    fd_temp = xmkstemp(temp_name);

    block_sig(SIGUSR1);
    block_sig(SIGUSR2);
    int childID = fork();
    if (childID == 0)
        P2();
    else
        P1(childID);

    wait(NULL);
    return 0;
}

int read20bytes(int fd, struct line* l);
int write10points(int fd, struct line* l);
int line_equation(struct line* l);
int read10points(int fd, struct line* l);
int read_line(int fd, char *buf);
struct point buf2point(char **buf);
struct point buf2equation(char **buf);
int remove_line(int fd);

void file_lock(int fd);
void file_unlock(int fd);


void register_sighandler(int sig, void (*fun_ptr)(int)){
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = fun_ptr;
    sigaction (sig, &sa, NULL);
}


void p1_sigterm_handler(int sig){
    printf("\nSIGTERM catched: %d. Cleaning...", sig);
    close(fd_in);
    close(fd_temp);
    exit(EXIT_SUCCESS);
}

sigset_t signals_arrived;
struct sigaction prev[_NSIG];

void critical_handler(int sig){
    sigaddset(&signals_arrived, sig);    
}

void print_signal_counter(){
    int none = TRUE;
    printf("\nSignals came in the critical section:");
    for (int i = 1; i < _NSIG; i++) {
        if (sigismember(&signals_arrived, i)){
            printf("\n%s.", strsignal(i));
            none = FALSE;
        }
    }
    if (none)
        printf(" None."); 
    printf("\n");
}

// registers critical_handler to all signals.
void register_signal_counter(int unregister){
    struct sigaction sa_counter; 
    sigemptyset(&sa_counter.sa_mask);
    sa_counter.sa_flags = 0;
    sa_counter.sa_handler = &critical_handler;

    if (unregister){
        for (int i=1;i<_NSIG; ++i){
            if (i != SIGTERM)
                sigaction (i, &prev[i], NULL);
        }
    }
    else {
        for (int i=1;i<_NSIG; ++i) {
            if (i != SIGTERM)
                sigaction (i, &sa_counter, &prev[i]);
        }
    }
       

}


void P1(int childID){
    int i = 0;
    struct line l; // no need for initialize.

    unblock_sig(SIGUSR1);
    unblock_sig(SIGUSR2);
    register_sighandler(SIGTERM, &p1_sigterm_handler);
    sigemptyset(&signals_arrived);
    sigset_t count_signals;
    sigfillset(&count_signals);

  
    while (read20bytes(fd_in, &l) == BYTES){
        printf("\nP1 is producing...\n");
        // sleep(1); 
        register_signal_counter(FALSE); // stores the signals that has come inside.
        block_sig(SIGINT);
        block_sig(SIGSTOP);
        /* signal critical section */
        line_equation(&l);
        /* signal critical section */
        unblock_sig(SIGINT);
        unblock_sig(SIGSTOP);
        register_signal_counter(TRUE);

        /* file critical section */
        write10points(fd_temp, &l);
        if (kill(childID, SIGUSR2) == -1) // wake up the consumer.
            xerror(__func__);
        /* end of file critical section */
        ++i;
    }

    close (fd_in);
    close (fd_temp);

    printf("\nP1 finished. Written bytes: %d. Calculated equations: %d\tOK.", i*BYTES, i);
    print_signal_counter();
    fflush(stdout);
    if (kill (childID, SIGUSR1) == -1) // signal to consumer that p1 is done.
        xerror(__func__);

}


void errors(struct line* l, struct metrics_stats* stats);
int write10points_errors(int fd, struct line* l);
void print_mean (struct metrics_stats* stats);
void print_SD (struct metrics_stats* stats);

int P1_finished = FALSE;

void handle_usr1(int sig){
    printf ("\nP1 has send a finish signal to P1! %d\n", sig);
    P1_finished = TRUE; // P1 has finished.
}

struct metrics_stats stats; // defined globally to do the cleanup in case of SIGTERM in P2

void p2_sigterm_handler(int sig){
    printf("\nSIGTERM catched: (SIG: %d). Cleaning...", sig);
    unlink(temp_name); // deletes the temp file.
    close (fd_out);
    free(stats.metrics);
    exit(EXIT_SUCCESS);
}

void handle_usr2 (int signal_number) {
    printf ("\nP2 awakens. Continuing to consume... (SIG: %d) \n", signal_number);
}


void P2(){
    struct line l;

    stats.capacity = 10;
    stats.metrics = (struct metric*) xmalloc (sizeof (struct metric) * stats.capacity); // initial size.
    stats.idx = 0;

    register_sighandler(SIGUSR1, &handle_usr1);
    register_sighandler(SIGUSR2, &handle_usr2);
    register_sighandler(SIGTERM, &p2_sigterm_handler);

    unblock_sig(SIGUSR1); // unblock SIGUSR1, since now it has an handler.
 

    int fd_in_p2 = xopen(temp_name, O_RDWR);
    int i = 0;
    while (read10points(fd_in_p2, &l)){
        printf("\nP2 is consuming...\n");
        /* end of file critical section */
        ++i;

        block_sig(SIGINT);
        block_sig(SIGSTOP);
        /* critical section */

        errors(&l, &stats);

        /* critical section */
        unblock_sig(SIGINT);
        unblock_sig(SIGSTOP);
        

        write10points_errors(fd_out, &l);
        /* file critical section */
    }
    
    printf("\nP2 has finished!\n");
    print_mean(&stats);
    print_SD(&stats);

    unlink(temp_name); // deletes the temp file.

    close (fd_in_p2);
    close (fd_out);
    free(stats.metrics);
    exit(EXIT_SUCCESS);
}

void file_lock(int fd){
    struct flock lck = {
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 1,
    };

    lck.l_type = F_WRLCK;
    if (fcntl (fd, F_SETLKW, &lck) == -1)
        xerror(__func__);
    
}

void file_unlock(int fd){
    struct flock lck = {
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 1,
    };

    lck.l_type = F_UNLCK;
    if (fcntl (fd, F_SETLK, &lck) == -1)
        xerror(__func__);
    
}

void block_sig(int sig){
    sigset_t block;
    if (sigemptyset(&block) == -1)
        xerror(__func__);
    if (sigaddset(&block, sig) == -1)
        xerror(__func__);
    if (sigprocmask(SIG_BLOCK, &block, NULL) == -1)
        xerror(__func__);

}

void unblock_sig(int sig){
    sigset_t unblock;
    if (sigemptyset(&unblock) == -1)
        xerror(__func__);
    if (sigaddset(&unblock, sig) == -1)
        xerror(__func__);
    if (sigprocmask(SIG_UNBLOCK, &unblock, NULL) == -1)
        xerror(__func__);
}

float mean (struct metrics_stats* stats, int flag){
    assert (flag >= 0 && flag < 3);
    float means [3];
    memset(means, 0, (3 * sizeof (float)));
    for (unsigned int i = 0; i < stats->idx; ++i){
        means[0] += stats->metrics[i].mae;
        means[1] += stats->metrics[i].mse;
        means[2] += stats->metrics[i].rmse;
    }

    return means[flag] / stats->idx;
}

float SD (struct metrics_stats* stats, int flag){
    assert (flag >= 0 && flag < 3);
    float SDs[3], means[3];
    memset (SDs, 0, 3 * sizeof (float));

    for (int i = 0; i < 3; ++i)
        means[i] = mean (stats, i);

    for (unsigned int i = 0; i < stats->idx; ++i){
        SDs[0] += pow (stats->metrics[i].mae - means[0], 2);
        SDs[1] += pow (stats->metrics[i].mse - means[1], 2);
        SDs[2] += pow (stats->metrics[i].rmse - means[2], 2);
    }

    return sqrt(SDs[flag]/(stats->idx-1));
}

void print_mean (struct metrics_stats* stats){

    printf ("\n\nMean:\t\tMAE\t\tMSE\t\tRMSE\n\t\t%f\t%f\t%f\n", 
    mean(stats, 0), mean(stats, 1), mean(stats, 2));

}

void print_SD (struct metrics_stats* stats){

    printf ("\n\nSD:\t\tMAE\t\tMSE\t\tRMSE\n\t\t%f\t%f\t%f\n\n", 
    SD(stats, 0), SD(stats, 1), SD(stats, 2));

}

// turns given 20 bytes to 10 points.
void bytes2points(char* buf, struct point* points){
    for (size_t i = 0; i < BYTES; i += 2){
        points[i/2].x = (unsigned int) buf[i]; // they will be interpreted as unsigned int.
        points[i/2].y = (unsigned int) buf[i+1];
    }
    
}


// moves the cursor to the next empty space for p1.
void next_empty(int fd){
    xlseek(fd, 0, SEEK_SET); 
    char buf[2];
    int read = 0;
    int offset = 0;
    while((read = xread(fd, buf, 2)) > 0){
        offset += 2;
        if (read == 1 && buf[0] == SPACE){
            xlseek(fd, offset-2, SEEK_SET);
            break;
        }
        if (buf[0] == SPACE && buf[1] == SPACE){
            xlseek(fd, offset-2, SEEK_SET);
            break;
        }
        else if (buf[0] == NEWLINE_DELIMETER && buf[1] == SPACE){
            xlseek(fd, offset-1, SEEK_SET);
            break;
        }      
    } 

}


// it reads the next 20 bytes from then given fd. state of cursor does matter.
int read20bytes (int fd, struct line* l) {
    char buf[BYTES];
    int read = xread(fd, buf, BYTES);
    if (read == BYTES)
        bytes2points(buf, l->points);
    return read;
}


// could be inside critical section.
int line_equation(struct line* l){
    float Exy = 0, Ex = 0, Ey = 0, Ex2 = 0;
    const int N = BYTES / 2;

    struct point* points = l->points;
    for (int i = 0; i < N; i++){ 
        Exy += points[i].x * points[i].y;
        Ex += points[i].x;
        Ey += points[i].y;
        Ex2 += points[i].x * points[i].x;  
    }
    
    float m = (N*Exy - (Ex * Ey)) / (N*Ex2 - (Ex*Ex)); // calculate slope.
    float b = (Ey - (m*Ex)) / N; // calculate intercept.

    points[N].x = m;
    points[N].y = b;

    return N;
}

// writes points to give string. 
int points2bytes(char *buf, struct line *l, int errors){
    const int N = BYTES / 2;
    char temp[256]; // inter buffer to adjust formats and strings.
    char *offset = buf;
    int bytes = 0;

    for (int i = 0; i < N+1; ++i){ // write points.
        if (i == N)
            sprintf(temp, "%.3fx%+.3f%c", l->points[i].x, l->points[i].y, NEWLINE_DELIMETER);
        else
            sprintf(temp, "(%u, %u), ", (unsigned int) l->points[i].x, (unsigned int) l->points[i].y);

        sprintf(offset, "%s", temp);
        bytes += strlen(temp);
        offset += strlen(temp);
    }

    if (errors){ // print error next to it.
        --offset; // overwrite new line.
        sprintf(temp, ", %.3f, %.3f, %.3f\n", l->mae, l->mse, l->rmse);
        sprintf(offset, "%s", temp);
        bytes += strlen(temp)-1;
    }

    return bytes;
}

// write the points to given fd.
int write10points(int fd, struct line* l){
    char buf[512];
    const int bytes = points2bytes(buf, l, FALSE);
    file_lock(fd);
    next_empty(fd);
    int written = xwrite(fd, buf, bytes);
    file_unlock(fd);
    return written;
}

// moves the cursor to the next non-empty space for p2.
void next_available(int fd){
    xlseek(fd, 0, SEEK_SET); 
    char buf[2];
    int read = 0;
    int offset = 0;
    while((read = xread(fd, buf, 2)) > 0){
        offset += 2;
        if (buf[0] != SPACE){ 
            xlseek(fd, offset-2, SEEK_SET);
            break;
        }
        else if (buf[0] == NEWLINE_DELIMETER && buf[1] != SPACE){
            xlseek(fd, offset-1, SEEK_SET);
            break;
        }        
    } 
}

int read10points(int fd, struct line* l){
    char buf[512]; // at most 512 byte for a line
    const int N = BYTES / 2;
    int cont = FALSE;
    struct point* points = l->points;

    file_lock(fd);
    next_available(fd);
    cont = read_line(fd, buf);
    file_unlock(fd);
    if (cont) { // continue if there is input.
        char *temp = buf + 1;
        for (int i = 0; i < N; ++i)
            points[i] = buf2point(&temp);
        points[N] = buf2equation(&temp);
        file_lock(fd);
        remove_line(fd);
        file_unlock(fd);
    }
    else if (!P1_finished){ // there is no input. wait for a signal.
        printf("\nNo item has found!. P2 is suspending...\n");
        sigset_t sigusr;
        if (sigfillset (&sigusr) == -1) xerror(__func__);
        if (sigdelset (&sigusr, SIGUSR2) == -1) xerror(__func__);
        if (sigdelset (&sigusr, SIGUSR1) == -1) xerror(__func__);
        sigsuspend (&sigusr); // wait for SIGUSR2.
        block_sig(SIGUSR2); // block again.
        return read10points(fd, l);
    }
    return cont;
}

// reads one line containing of points.
int read_line(int fd, char *buf){
    off_t off = 0;
    int cont = FALSE;
    while (xread(fd, buf, 1) > 0) {
        ++off;
        buf[off] = buf[0];
        if (buf[0] == NEWLINE_DELIMETER){
            cont = TRUE; 
            break;
        }
        if (off >= 512){
            assert (buf[1] == ' ');
            cont = FALSE;
            break;
        }

        assert (off <= 512);
    }

    return cont;
}

// turns given "(x, y), ..." to a struct. increments the cursor.
struct point buf2point(char **buf){
    struct point p;
    char *end_ptr;

    assert (**buf == '(');
    *buf = *buf + 1; // pass '('.
    p.x = str2int(*buf, &end_ptr); // read x.
    assert(*end_ptr == ','); 
    *buf = end_ptr + 2; // pass ',' and space.

    p.y = str2int(*buf, &end_ptr); // read y.
    assert(*end_ptr == ')');
    *buf = end_ptr + 3; // pass ')', ',' and space.
    return p;
}

// returns given "ax+b" to a struct.
struct point buf2equation(char **buf){
    struct point p;
    int MINUS = **buf=='-'? TRUE : FALSE;
    char *end_ptr; 

    p.x = str2int(*buf, &end_ptr); // read first par t of a.
    if (*end_ptr == '.'){
        *buf = end_ptr + 1;
        if (MINUS)
            p.x -= (float) str2int(*buf, &end_ptr) / 1000;
        else 
            p.x += (float) str2int(*buf, &end_ptr) / 1000; 
    }
    assert(*end_ptr == 'x');
    *buf = end_ptr + 1;

    MINUS = **buf=='-'? TRUE : FALSE;
    p.y = str2int(*buf, &end_ptr); // read b
    if (*end_ptr == '.'){
        *buf = end_ptr + 1;
        if (MINUS)
            p.y -= (float) str2int(*buf, &end_ptr) / 1000;
        else 
            p.y += (float) str2int(*buf, &end_ptr) / 1000; 
    }
    assert(*end_ptr == NEWLINE_DELIMETER);
    return p;
}

// f(x) = ax + b. p {a, b}.
float f(float x, struct point p){
    return x * p.x + p.y;
}

void update_stats (struct line* l, struct metrics_stats* stats){
    if (stats->idx == stats->capacity){
        stats->capacity *=2;
        stats->metrics = (struct metric*) xrealloc (stats->metrics, stats->capacity* sizeof (struct metric)); 
    }

    stats->metrics[stats->idx].mae = l->mae;
    stats->metrics[stats->idx].mse = l->mse;
    stats->metrics[stats->idx].rmse = l->rmse;


    ++stats->idx;
}

void errors(struct line* l, struct metrics_stats* stats){
    const int N = BYTES / 2;
    struct point eq = l->points[N]; // get line equation.
    l->mae = 0;
    l->mse = 0;
    l->rmse = 0;
    for (int i = 0; i < N; ++i){
        l->mae += fabs(f(l->points[i].x, eq) - l->points[i].y); 
        l->mse += pow(f(l->points[i].x, eq) - l->points[i].y, 2); 
    }
    l->mae /= N;
    l->mse /= N;
    l->rmse = sqrt(l->mse);

    update_stats(l, stats);

}


// writes 10 points with their errors to given fd.
int write10points_errors(int fd, struct line *l){
    char buf[512];
    const int bytes = points2bytes(buf, l, TRUE);

    return xwrite(fd, buf, bytes);
}

// learns the size of the given file. cursor will change after calling this.
int size(int fd){
    const int READ_SIZE = 512; // it will read 512 bytes each.
    char buf[512];
    xlseek(fd, 0, SEEK_SET); // move to cursor to the beginning.
    int _size = 0;

    int rd = 0;
    while ((rd = xread(fd, buf, READ_SIZE)) > 0)
        _size += rd;
        
    return _size;
}


// removes the first line of the given file fd.
int remove_line(int fd){    
    int _size = size(fd);
    char *old_buf = (char *) xmalloc(_size);
    char *new_buf = (char *) xmalloc(_size);
    memset(new_buf, (int) SPACE, _size);

    // get current file.
    xlseek(fd, 0, SEEK_SET);
    xread(fd, old_buf, _size);

    // don't include the first in the new buffer and it's size.
    char * buffer_without_first = strchr(old_buf, NEWLINE_DELIMETER);
    memcpy(new_buf, buffer_without_first+1, _size - (buffer_without_first - old_buf + 1));

    xlseek(fd, 0, SEEK_SET);
    xwrite(fd, new_buf, _size); 
    xlseek(fd, 0, SEEK_SET);
    free(new_buf);
    free(old_buf);
    return TRUE;
}
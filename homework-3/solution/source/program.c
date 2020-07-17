/**
* receives 2 input square matrices. distribute the calculation of their product to its children, 
* collect from them the partial outputs, combine them, and then calculate the singular values of
* the product matrix.
*/

#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include "utils.h"

#define CHILDREN 4


typedef enum {
    QUARTER1ST,
    QUARTER2ND,
    QUARTER3RD,
    QUARTER4TH
} quarters;

typedef enum {
    READ, // reading end.
    WRITE // writing end.
} ends;

typedef enum {
    P, // source is parent dest is child.
    C // vice versa.
} pipe_type;

struct matrix { // square matrix nxn.
    float **buffer;
    int i, j;
};

struct packet {
    struct matrix *mats; // two matrix operands.
    size_t size;
};


void buf2matrix(char *buf, struct matrix* mat);
struct matrix create_matrix(int i, int j);
void delete_matrix(struct matrix* mat);
void print_matrix (struct matrix* mat);
void set_item(struct matrix* mat, int i, int j, float item);
float get_item(struct matrix* mat, int i, int j);
void read_input(int fd, struct matrix* mat);
void quarter(int fd[2][2], quarters q);
struct matrix piece(struct matrix* mat, int imin, int imax, int jmin, int jmax);
void write_operands(int fd, struct matrix* mat1, struct matrix* mat2, quarters qua);
int pack(struct matrix* mat, char **pkt);
int unpack(struct matrix* mat, char *pkt);
struct matrix multiply(struct matrix* mat1, struct matrix* mat2);
int read_matrix(int fd, struct matrix *mats, int n);
struct matrix merge_quarters(struct matrix mats[CHILDREN]);
void assign_quarters(int fd[CHILDREN][2][2], struct matrix* a, struct matrix* b);
struct matrix collect_quarters(int fd[CHILDREN][2][2]);
void svd(struct matrix *mat);
void P1(int in1_fd, int in2_fd, int n);
struct matrix get_quarter(struct matrix* mat, quarters qua);

int main(int argc, char **argv)
{
    int in1_fd, in2_fd, n; // input files and n.

    // 5 processes will be writing simultaneously to stdout. better we see the outcome in the correct order.
    setvbuf(stdout, NULL, _IONBF, 0); 
    parse_args (argc, argv, &in1_fd, &in2_fd, &n);

    P1 (in1_fd, in2_fd, (int) pow (2, n));

    return 0;
}

int zombies_handled = 0;

void child_handler(int sig){
    if (waitpid(-1, &sig, WNOHANG) == -1)
        xerror(__func__, "handling SIGCHILD.");
    else{
        ++zombies_handled;
        return;
    }
    
}

void int_handler(int sig){
    printf("\n[ ALL ] CTRL + C (%d). Exiting...", sig);
    exit(EXIT_FAILURE);
}

void P1(int in1_fd, int in2_fd, int n){
    struct matrix a = create_matrix (n, n);
    struct matrix b = create_matrix (n, n);

    read_input (in1_fd, &a);
    read_input (in2_fd, &b);

    int fd[CHILDREN][2][2]; // bi-directional pipe for each child. 

    signal (SIGCHLD, child_handler);
    signal (SIGINT, int_handler); 

    printf("\n[ P1 ] Starting...");
    assign_quarters (fd, &a, &b); // create 4 process and assign a quarter for each.
    printf("\n[ P1 ] Finished assigning quarters. Collecting quarters...");
    struct matrix final = collect_quarters (fd);
    printf("\n[ P1 ] Collected all quarters. OK.\n");
    print_matrix (&final);
     
    svd(&final);

    delete_matrix (&final);
    delete_matrix (&a);
    delete_matrix (&b);

    close (in1_fd);
    close (in2_fd);

    if (zombies_handled < CHILDREN) // block if the childrens still alive.
        pause();
    printf("\n[ P1 ] Exiting...\n");
    
}



// creates a bidirectional pipe and ... // vquarters qua
int pipe_and_fork(int fd[2][2], quarters q){
    if (pipe(fd[P]) == -1) 
        xerror (__func__, "while creating first pipe.");
    if (pipe(fd[C]) == -1)
        xerror (__func__, "while creating second pipe.");
    
    pid_t p = fork ();
    if (p < 0)
        xerror (__func__, "fork failed.");
    else if (p == 0){ // child.
        close (fd[P][WRITE]); // close writing end of reading pipe.
        close (fd[C][READ]); // close reading end of writing pipe.
        quarter (fd, q);
    }
    else { // parent.
        close (fd[C][WRITE]);
        close (fd[P][READ]);
    }
    return p; 
}

// reads quarter from the pipe to given matrix. matrix must be empty.
void read_quarter(int fd, struct matrix mats [2]){
    read_matrix (fd, mats, 2);
}

// writes the calculated quarter to the pipe.
void write_quarter(int fd, struct matrix* mat){
    char *pkt;
    int pkt_size = pack (mat, &pkt); // pack the matrix

    if ((xwrite (fd, (char *) pkt, pkt_size)) != pkt_size)
        xerror (__func__, "while writing quarter");
    free (pkt);
}

// job of each child. must be called by different children processes.
void quarter(int fd[2][2], quarters q){
    struct matrix mats[3];

    printf("\n[ Q%d ] Starting...", q);
    read_quarter (fd[P][READ], mats); // read matrices from pipe.
    printf("\n[ Q%d ] Finished reading operands from pipe.", q);
    mats[2] = multiply (&mats[0], &mats[1]); // multiply the pieces.
    printf("\n[ Q%d ] Quarter calculated.", q);
    write_quarter (fd[C][WRITE], &mats[2]); // return output.
    printf("\n[ Q%d ] Exiting...", q);

    close (fd[P][READ]);
    close (fd[C][WRITE]);
    exit(EXIT_SUCCESS);
}


void assign_quarters(int fd[CHILDREN][2][2], struct matrix *a, struct matrix *b){

    for (quarters q = QUARTER1ST; q < CHILDREN; ++q){
        pipe_and_fork (fd[q], q);
        write_operands(fd[q][P][WRITE], a, b, q);
        printf("\n[ P1 ] Writed operands to Q%d.", q);
    }
} 

// collects quarters from children to the given matrix.
struct matrix collect_quarters(int fd[CHILDREN][2][2]){
    struct matrix mats[CHILDREN];

    for (quarters q = QUARTER1ST; q < CHILDREN; ++q)
        read_matrix (fd[q][C][READ], &mats[q], 1);
    
    // reaching here means quarters have been collected. children did the calculation.
    for (quarters q = QUARTER1ST; q < CHILDREN; ++q){
        close (fd[q][C][READ]);
        close (fd[q][P][WRITE]);
    }
    return merge_quarters (mats);
}

// merges the quarters to their appropriate place.
struct matrix merge_quarters(struct matrix mats[CHILDREN]){
    int ni = mats[0].i;
    int nj = mats[0].j;

    struct matrix final = create_matrix(ni * 2, nj * 2);
    int shifti[] = {0, 0, 0, 0};
    int shiftj[] = {0, 0, 0, 0};

    shifti[QUARTER3RD] = ni;
    shifti[QUARTER4TH] = ni;

    shiftj[QUARTER2ND] = nj;
    shiftj[QUARTER4TH] = nj;


    for (quarters q = QUARTER1ST; q < CHILDREN; ++q){
        for (int i = 0; i < ni; ++i){
            for (int j = 0; j < nj; ++j)
                set_item (&final, i+shifti[q], j+shiftj[q], get_item (&mats[q], i, j));
        }
    }

    return final;
}

// reads 2^n characters from the file to given matrix. 
void read_input(int fd, struct matrix* mat){
    int read_byte = mat->i * mat->j; // 2^n. x 2^n
    char *buf = xmalloc (read_byte);

    if (xread (fd, buf, read_byte) != read_byte)
        xerror (__func__, "could not read 2^n bytes.");

    buf2matrix (buf, mat);
    free (buf);
}


// sets the read buffer to given matrix.
void buf2matrix(char *buf, struct matrix* mat){
    for (int i = 0; i < mat->i; ++i)
        for (int j = 0; j < mat->j; ++j)
            set_item(mat, i, j, (int) buf[i * mat->j + j]); 
}

// creates a matrix. delete_matrix() should be called after calling this.
struct matrix create_matrix(int i, int j){
    struct matrix mat;
    mat.buffer = (float **) xmalloc (i * sizeof (int*));
    for (int k = 0; k < i; ++k)
        mat.buffer[k] = (float *) xmalloc (j * sizeof (int));
    mat.i = i;
    mat.j = j;
    return mat;
}

// frees the matrix.
void delete_matrix(struct matrix* mat){
    assert (mat->buffer != NULL);
    for (int i = 0; i < mat->i; ++i)
        free (mat->buffer[i]);
    free (mat->buffer);
}

// prints the matrix with style.
void print_matrix (struct matrix* mat){
    assert (mat->buffer != NULL);
    printf ("\n{\t[mat: %dx%d]\n\n", mat->i, mat->j);
    for (int i = 0; i < mat->i; ++i){
        printf ("|  ");
        for (int j = 0; j < mat->j; ++j)
            printf ("%7.3f ", mat->buffer[i][j]);
        printf ("  |\n");
    }
    printf ("\n}\n");    
}

// gets the (i, j) in matrix.
float get_item(struct matrix* mat, int i, int j){
    assert (i <= mat->i && i >= 0 && j <= mat->j && j >= 0);
    assert (mat->buffer != NULL);
    return mat->buffer[i][j];
}

// sets the (i, j) int matrix.
void set_item(struct matrix* mat, int i, int j, float item){
    assert (i <= mat->i && i >= 0 && j <= mat->j && j >= 0);
    assert (mat->buffer != NULL);
    mat->buffer[i][j] = item;
}

// multiplies two matrix. 
struct matrix multiply(struct matrix* a, struct matrix* b){
    assert (a->j == b->i);
    struct matrix c = create_matrix (a->i, b->j);

    int ai = a->i;
    int aj = a->j;
    int bj = b->j;
    
    for (int i = 0; i < ai; ++i){
        for (int j = 0; j < bj; ++j){
            int cij = 0;
            for (int x = 0; x < aj; ++x)
                cij += get_item (a, i, x) * get_item (b, x, j); 
            set_item (&c, i, j, cij);
        }
    }

    return c;
}

// returns two operands needed for to calculate the desired quarter.
void operands(struct matrix* a, struct matrix* b, struct matrix mats[2], quarters qua){
    assert (a->i && a-> j);
    int n = a->i;
    int halfn = n / 2;

    switch (qua){
        case QUARTER1ST:
            mats[0] = piece (a, 0, halfn, 0, n);
            mats[1] = piece (b, 0, n, 0, halfn);
            break;
        case QUARTER2ND:
            mats[0] = piece (a, 0, halfn, 0, n);
            mats[1] = piece (b, 0, n, halfn, n);
            break;
        case QUARTER3RD:
            mats[0] = piece (a, halfn, n, 0, n);
            mats[1] = piece (b, 0, n, 0, halfn);
            break;
        case QUARTER4TH:
            mats[0] = piece (a, halfn, n, 0, n);
            mats[1] = piece (b, 0, n, halfn, n);
            break;
        default:
            xerror(__func__, "wrong quarter number.");            
    }

}


// gets a partial matrix from given matrix. [imin, imax). [jmin, jmax).
struct matrix piece(struct matrix* mat, int imin, int imax, int jmin, int jmax){
    assert (imin>=0 && jmin>=0 && imax<=mat->i && jmax<=mat->j);
    struct matrix partial = create_matrix(imax - imin, jmax - jmin);
    
    for (int i = imin; i < imax; ++i)
        for (int j = jmin; j < jmax; ++j)
            set_item (&partial, i - imin, j - jmin, get_item (mat, i, j));
    return partial;
}

int read_matrix(int fd, struct matrix *mats, int n){
    assert (n == 2 || n == 1);
    for (int i = 0;i < n; ++i){
        int initial_read_size = 10;
        char *pkt = (char *) xmalloc (initial_read_size); // only for dims.

        int byte = xread (fd, pkt, initial_read_size);

        // since execution is blocked. pipe must be ready. 
        assert (byte != EOF && byte != 0);
           
        if (byte != initial_read_size){ // first learn the dims of the matrix.
            free (pkt);
            xerror (__func__, "while reading size");
        }

        // if reached here, there is a matrix to read in the pipe.
        
        char *end_ptr;
        int pkt_size = str2int(pkt, &end_ptr); // size of the whole pkt.
        int header_size = end_ptr - pkt + 1;
        int pkt_already_read_size = initial_read_size - header_size;

        pkt = (char *) xrealloc (pkt, pkt_size + header_size); 
        int rest_pkt_size = pkt_size - pkt_already_read_size;

        if ((xread (fd, pkt + initial_read_size, rest_pkt_size)) != rest_pkt_size)
            xerror (__func__, "while reading matrix buffer");

        unpack (&mats[i], (char *) pkt);
        free (pkt);
    }
    
    return FALSE;
}


// writes 2 matrix to given fd.
void write2matrix(int fd, struct matrix mats[]){
    char *pkts[2];
    int size = 0;

    for (int i = 0; i<2; ++i)
        size += pack (&mats[i], &pkts[i]);
    
    
    
    pkts[0] = (char *) xrealloc (pkts[0], size);
    strncat (pkts[0], pkts[1], size);


    if (xwrite (fd, pkts[0], size) != size)
        xerror (__func__, "writing the merged matrix.");
    
    for (int i=0; i<2; ++i)
        free (pkts[i]);

}

// it will send the appropriate operands to the desired quarter to pipe.
void write_operands(int fd, struct matrix* a, struct matrix* b, quarters qua){
    struct matrix mats[2]; // one row and one column matrix will be sent to pipe as array.

    operands(a, b, mats, qua); // get operands

    write2matrix (fd, mats); // write operands.

    delete_matrix (&mats[0]);
    delete_matrix (&mats[1]);
}

// packs to matrix to given buffer with a pre-defined protocol.
// returned packet must be freed by the user.
// protocol is: {i, j, buffer[i][j]}. 
int pack(struct matrix* mat, char **pkt){
    int size = mat->i * mat->j + 2;
    float *buf = (float *) xmalloc (size * sizeof (float));

    buf[0] = mat->i;
    buf[1] = mat->j;
    for (int i = 2;i < size; ++i)
        buf[i] = mat->buffer[(i-2)/mat->j][(i-2)%mat->j];
    
    // reasonable arbitrary number. we will find the correct one and change it soon.
    int initial_size = size * 8; 
    *pkt = (char *) xmalloc (initial_size);
    char *offset = *pkt;
    int total = 0;
    char temp[20];
    for (int i = 0;i < size; ++i){
        sprintf (temp, "%.3f;", buf[i]);
        sprintf (offset, "%s", temp);
        offset += strlen (temp);
        total += strlen (temp);

        if (total >= initial_size){ // enlargement if needed.
            int diff = offset - *pkt;
            initial_size *=2;
            *pkt = (char *) xrealloc (*pkt, initial_size);
            offset = *pkt + diff;
        }
    }

    // put the total size into beginning.
    sprintf (temp, "%d;", total);
    total += strlen (temp);
    char * pkt_with_size = (char *) xmalloc (total);
    sprintf (pkt_with_size, "%s", temp);
    strncpy (pkt_with_size + strlen (temp), *pkt, total - strlen(temp));
    free (*pkt);
    *pkt = pkt_with_size;

    
    free (buf);        
    return total;
}

// unpacks the given packet to a matrix. see the pack function.
// matrix must be empty. see delete_matrix.
int unpack(struct matrix* mat, char *pkt){
    char *end_ptr;
    str2int (pkt, &end_ptr); // pass the total byte size.
    int i = (int) str2float (end_ptr+1, &end_ptr);
    int j = (int) str2float (end_ptr+1, &end_ptr);
    
    *mat = create_matrix (i, j);
    int size = mat->i * mat->j + 2;

    for (int i = 2; i < size; ++i)
        mat->buffer[(i-2)/mat->j][(i-2)%mat->j] = str2float (end_ptr+1, &end_ptr);
    return size;
}

// applies singular value decomposition to given matrix. returns the resultant matrix.
// encapculates the external svd() function and adapts to our model used in here. 
void svd(struct matrix *mat){
    assert (mat->i == mat->j);
    int n = mat->i;

    /* allocs */
    float *w = (float *) xmalloc (n * sizeof (float));
    float **v = (float **) xmalloc (n * sizeof (float*));
    for (int i = 0; i < n; ++i) v[i] = (float *) xmalloc (n * sizeof (float));


    compute_svd (mat->buffer, n, n, w, v);

    printf("Singular Values: ");
    for (int i = 0; i<n; ++i)
        printf ("%.3f ", w[i]);
    free(w);
    for (int i = 0; i < n; ++i) free (v[i]);
    free(v);
}
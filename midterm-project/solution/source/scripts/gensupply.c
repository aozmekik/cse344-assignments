#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

// @see program.c
// generates supply to a supply.txt file for supplier.
// this .c file is only created for testing purposes. 
// not internal part of the homework, therefore it is not designed in a generic way.

#define DESERT 'D'

enum plate
{
    P, // soup
    C, // main course
    D, // desert.
    EMPTY,
};


#define SOUP 'P'
#define COURSE 'C'
#define DESERT 'D'
#define SUPPLIES "supply.txt"
#define FOOD_KIND 3
// #define L 4
// #define M 6

void gen_supply(int L, int M);
int str2int(const char *buf);

int main(int argc, char const *argv[])
{   
    srand(time(NULL));
    printf("-L %s -M %s\n", argv[1], argv[2]);
    gen_supply(str2int(argv[1]), str2int(argv[2]));
    
    return 0;
}

int str2int(const char *buf)
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



enum plate rand_food(int food_delivered[FOOD_KIND])
{
    enum plate p;

    do
        p = rand() % FOOD_KIND;
    while (food_delivered[p] == 0); // take it again if already delivered enough.
    return p;
}



// generates supply for the supplier to a determined file.
void gen_supply(int L, int M)
{
    int total = L * M;
    int food_delivered[FOOD_KIND] = {total, total, total};
    total *= FOOD_KIND;
    char *supply = (char *)malloc(total);

    printf("Generating: \n");
    for (int i = 0; i < total; ++i)
    {
        char food;
        enum plate p;
        switch (p =  rand_food(food_delivered)) // generate randomly from each kind.
        {
        case P:
            food = SOUP;
            break;
        case C:
            food = COURSE;
            break;
        case D:
            food = DESERT;
            break;
        default: assert(0);
        }
        supply[i] = food;
        --food_delivered[p];
        printf("%c ", supply[i]);
    }

    
    int fd = open(SUPPLIES, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
        perror("open");
    if (write(fd, supply, total) != total)
        perror("write");
    close(fd);
    free(supply);
    printf("\nOK.\n");
}

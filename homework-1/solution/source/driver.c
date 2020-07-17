//
// Created by drh0use on 3/14/20.
//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void create_process(int x, char** args){
    int pid;
    if ((pid = fork()) == -1){
        fprintf(stderr, "Can't fork.");
        exit(EXIT_FAILURE);
    }
    else if(pid == 0){

        printf("%s (%d) has just started...\n", args[0], x);
        execvp(args[0], args);
        exit(EXIT_SUCCESS);
    }

}

int main(void){

    printf("starting two programA...\n");
    char *args1[] = {"./programA", "-i", "inputa1.txt", "-o", "outputa.txt", "-t", "1"};
    char *args2[] = {"./programA", "-i", "inputa2.txt", "-o", "outputa.txt", "-t", "1"};

    char *args3[] = {"./programB", "-i", "outputa.txt", "-o", "outputb.txt", "-t", "1"};
    char *args4[] = {"./programB", "-i", "outputa.txt", "-o", "outputb.txt", "-t", "1"};


    create_process(1, args1);
    create_process(2, args2);

//    create_processA(1, args3);
//    create_processA(2, args4);



    return 0;
}


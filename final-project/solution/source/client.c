#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include "utils.h"

#define MAX_BYTE 1024

struct ClientArgs
{
    char host_addr[32];
    int port, src, dest;
};

void client_parse_args(int argc, char **argv, struct ClientArgs *args);
void check_arg(int flag, char arg);
void prepare_packet(struct ClientArgs *args, char *buf);
void client_help();
char *timestamp();

int main(int argc, char *argv[])
{
    pid_t pid = getpid();
    struct ClientArgs args;
    client_parse_args(argc, argv, &args);

    char packet[MAX_BYTE];

    prepare_packet(&args, packet);

    int sockfd;
    struct sockaddr_in host_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        xerror(__func__, "socket");

    bzero(&host_addr, sizeof(struct sockaddr_in));

    host_addr.sin_family = AF_INET;
    host_addr.sin_addr.s_addr = inet_addr(args.host_addr);
    host_addr.sin_port = htons(args.port);

    printf("[%s] Client (%d) connecting to %s:%d\n", timestamp(), pid, args.host_addr, args.port);
    if (connect(sockfd, (struct sockaddr *)&host_addr, sizeof(host_addr)) != 0)
        xerror(__func__, "client connect");

    printf("[%s] Client (%d) connected and requesting path from node %d to %d\n", timestamp(), pid, args.src, args.dest);
    xwrite(sockfd, packet, sizeof(struct Packet));

    int read_byte = 0;
    int first = TRUE;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    read_byte = xread(sockfd, packet, MAX_BYTE);
    do
    {
        if (read_byte <= 0)
            xerror(__func__, "client read");
        if (first)
        {
            // time(&end_t);
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            printf("[%s] Server's response to (%d): ", timestamp(), pid);
            first = FALSE;
        }
        char *end_ptr = strchr(packet, '.');
        if (end_ptr != NULL)
            end_ptr[0] = '\0'; // eliminate characters put by the server.
        printf("%s", packet);
    } while ((read_byte = xread(sockfd, packet, MAX_BYTE)) == MAX_BYTE);
    long delta = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    printf(", arrived in %f seconds. shutting down.\n", (float)delta / 1000000);

    close(sockfd);
}

void prepare_packet(struct ClientArgs *args, char *buf)
{
    struct Packet packet = {args->src, args->dest};
    memcpy(buf, &packet, sizeof(struct Packet));
}

void client_parse_args(int argc, char **argv, struct ClientArgs *args)
{
    int aflag = FALSE, pflag = FALSE, sflag = FALSE, dflag = FALSE;

    char opt;
    while ((opt = getopt(argc, argv, "a:p:s:d:")) != -1)
    {
        switch (opt)
        {
        case 'a':
            aflag = TRUE;
            strcpy(args->host_addr, optarg);
            break;
        case 'p':
            args->port = str_to_int(optarg);
            pflag = TRUE;
            if (args->port < 0 || args->port > 65535)
            {
                fprintf(stderr, "Port arg, is not in range [0, 65535].");
                exit(EXIT_FAILURE);
            }
            break;
        case 's':
            args->src = str_to_int(optarg);
            sflag = TRUE;
            if (args->src < 0)
            {
                fprintf(stderr, "Source node arg, is not in range [0, MAX_INT].");
                exit(EXIT_FAILURE);
            }
            break;
        case 'd':
            args->dest = str_to_int(optarg);
            dflag = TRUE;
            if (args->dest < 0)
            {
                fprintf(stderr, "Destination node arg, is not in range [0, MAX_INT].");
                exit(EXIT_FAILURE);
            }
            break;
        case '?':
        default:
            client_help();
            exit(EXIT_SUCCESS);
            break;
        }
    }
    check_arg(aflag, 'a');
    check_arg(pflag, 'p');
    check_arg(sflag, 's');
    check_arg(dflag, 'd');
}

void client_help()
{
    printf("Usage: ./client -a <server_address> -p <port> -s <src_node> -d <dest_node>\n"
           "Example: $./client -a 127.0.0.1 -p PORT -s 768 -d 979\n"
           "\t--help:\t\tdisplay what you are reading now\n\n"
           "Exis status:\n"
           "0\tif OK,\n"
           "1\tif minor problems (e.g., cannot find the file.), \n"
           "2\tif serious problems.\n\n\n");
}
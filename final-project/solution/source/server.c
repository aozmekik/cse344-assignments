#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <semaphore.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include "utils.h"
#include "graph.h"
#include "cache.h"

/* literals regarding to graph input */
#define NEWLINE_DELIMETER "\n"
#define COMMENT_DELIMETER '#'
#define TAB_DELIMETER '\t'

/* literals regarding to internal flow */
#define SEM_SINGLE_INSTANCE_NAME "sem-single-instance"
#define MAX_LOAD 0.75

/* A resource shared between server thread and the pool */
struct ConnHandlerResource
{
    struct Graph *graph;
    struct Queue *client_queue;

    /* sync for server main thread and connection handler threads*/
    sem_t *graph_mutex;
    sem_t *handler_sem;
    int finished;

    /* sync for cache read/write in connection handler threads */
    sem_t *read_try;
    sem_t *read_mutex, *write_mutex;
    int read_count, write_count;
    sem_t *cache_mutex;
    struct Cache *cache;
};

struct DynamicPoolerResource
{
    pthread_t *pool;
    int n; // number of threads.
    sem_t *pooler_sem;
    sem_t *load_mutex;
    pthread_t main_thread;
    int resize;
    int handler_count;
};

sem_t *single_instance_sem;

/* defined globally to achieve graceful termination in sigint */
struct Args args;
struct ConnHandlerResource *conr = NULL;
struct DynamicPoolerResource *dynr = NULL;

void become_daemon(); // become a daemon by following some routines.
void read_graph();    // load the graph to memory from input file.
void create_pool();
void attach_sigint_handler();
void init_shared_resources();
void destroy_shared_resources();

void connection_listener();       // function of server thread.
void *connection_handler(void *); // function of pool of threads.
void *pool_resizer(void *);       // function for thread handling dynamic pooling.
void create_sem();

int main(int argc, char *argv[])
{
    create_sem(); /* prevent double instanstation */
    parse_args(argc, argv, &args);
    become_daemon();
    init_shared_resources();
    attach_sigint_handler();
    read_graph();
    create_pool();
    connection_listener();

    return 0;
}

void create_sem();
void delete_sem();

int is_finished()
{
    xsem_wait(conr->graph_mutex);
    int finished = conr->finished;
    int clients = size(conr->client_queue);
    xsem_post(conr->graph_mutex);
    return finished && !clients;
}

void kill_thread(pthread_t tid)
{
    if (pthread_cancel(tid) != 0)
        xerror(__func__, "pthread_cancel");
    xthread_join(tid);
}

void handle_sigint()
{
    delete_sem();
    dprintf(args.outfd, "[%s] Termination signal received, waiting for ongoing threads to complete.\n", timestamp());

    if (dynr->pool != NULL)
    {
        xsem_wait(conr->graph_mutex);
        conr->finished = TRUE;
        xsem_post(conr->graph_mutex);

        pthread_t tid = pthread_self();

        if (tid != dynr->main_thread)
            kill_thread(dynr->main_thread);

        // wait for resizer thread.
        if (tid != dynr->pool[0])
            kill_thread(dynr->pool[0]);

        // wait for pool of handler threads to complete.
        xsem_post(conr->handler_sem);
        for (pthread_t i = 1; i < (unsigned)dynr->n; i++)
        {
            if (tid != i)
                xthread_join(dynr->pool[i]);
        }
    }
    dprintf(args.outfd, "[%s] All threads have terminated, server shutting down.\n", timestamp());
    destroy_shared_resources();
    exit(EXIT_SUCCESS);
}

void attach_sigint_handler()
{
    signal(SIGINT, handle_sigint);
}

void become_daemon()
{
    // @see main to prevent prevent double instantiation.

    // get rid of controlling terminals.
    switch (fork())
    {
    case -1:
        xerror(__func__, "first fork");
    case 0:
        break;
    default:
        exit(EXIT_SUCCESS);
    }

    if (setsid() == -1)
        xerror(__func__, "setsid");

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    switch (fork())
    {
    case -1:
        xerror(__func__, "second fork");
    case 0:
        break;
    default:
        exit(EXIT_SUCCESS);
    }

    // flushing file options.
    umask(0);

    // close all inherited open files.
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
        if (x != args.infd && x != args.outfd)
            close(x);
}

int read_raw(int fd, char **raw)
{
    int file_size = xlseek(fd, 0, SEEK_END); // learn the size of the file.
    *raw = (char *)xmalloc(file_size);

    xlseek(fd, 0, SEEK_SET); // rewind.
    if (xread(fd, *raw, file_size) != file_size)
        xerror(__func__, "file_size does not match!");
    xclose(fd);
    return file_size;
}

int is_comment(char *line)
{
    return line[0] == COMMENT_DELIMETER;
}

int max(int n1, int n2)
{
    return n1 > n2 ? n1 : n2;
}

// finds the number of vertices of the graph.
int find_V(char *raw, int byte)
{
    char *p = raw;
    int V = 0;
    while (p - raw != byte)
    {
        int i, j;
        if (!is_comment(p))
        {
            if ((i = str_to_int(p)) > V)
                V = i;
            if ((j = str_to_int(strchr(p, TAB_DELIMETER))) > V)
                V = j;
        }

        p = strchr(p, '\n') + 1;
    }
    return V + 1;
}

void read_graph()
{
    clock_t start, end;
    start = clock();
    dprintf(args.outfd, "[%s] Loading graph...\n", timestamp());
    char *raw;
    int byte = read_raw(args.infd, &raw);
    conr->graph = create_graph(find_V(raw, byte));
    conr->cache = create_cache(conr->graph->V);

    char *token = strtok(raw, NEWLINE_DELIMETER);
    int edge_count = 0;
    while (token != NULL) // walk through lines.
    {
        int i, j;
        if (!is_comment(token))
        {
            i = str_to_int(token);
            j = str_to_int(strchr(token, TAB_DELIMETER));
            add_edge(conr->graph, i, j);
            edge_count++;
        }
        token = strtok(NULL, NEWLINE_DELIMETER);
    }

    free(raw);
    end = clock();
    dprintf(args.outfd, "[%s] Graph loaded in %.6f seconds with %d nodes and %d edges.\n",
            timestamp(), (double)(end - start) / CLOCKS_PER_SEC, conr->graph->V, edge_count);
}

void create_sem()
{
    if ((single_instance_sem = sem_open(SEM_SINGLE_INSTANCE_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED)
    {
        if (errno == EEXIST)
        {
            printf("can't create a second instance of the server daemon!\n");
            printf("%s\n", SEM_SINGLE_INSTANCE_NAME);
            exit(EXIT_FAILURE);
        }
        xerror(__func__, "creating semaphore");
    }
}

void delete_sem()
{
    if (sem_close(single_instance_sem) == -1)
        xerror(__func__, "sem_close");
    if (sem_unlink(SEM_SINGLE_INSTANCE_NAME) == -1)
        xerror(__func__, "sem_unlink");
}

void connection_listener()
{
    int sockfd, clientfd;
    struct sockaddr_in host_addr, client_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == -1)
        xerror(__func__, "socket");

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
        xerror(__func__, "setsockopt");

    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(args.port);
    host_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(&(host_addr.sin_zero), '\0', 8); // zero the rest.

    if (bind(sockfd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr)) == -1)
        xerror(__func__, "bind");

    if (listen(sockfd, 20) == -1)
        xerror(__func__, "listen");

    while (TRUE)
    {
        socklen_t len = sizeof(struct sockaddr_in);
        if ((clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &len)) == -1)
            xerror(__func__, "accept");

        xsem_wait(conr->graph_mutex);
        enqueue(&conr->client_queue, clientfd);
        xsem_post(conr->graph_mutex);

        /* forward connection */
        xsem_post(conr->handler_sem);
    }
    exit(EXIT_SUCCESS);
}

char *prepare_packet(struct Queue *bfs);

long read_database(int i, int j);
void write_database(char *path, int i, int j);
float get_load();
int need_resize(float);

void *connection_handler(void *p)
{

    int packet_len = sizeof(struct Packet);
    char *recv_packet = xmalloc(packet_len);

    int *nth = (int *)p;

    while (TRUE)
    {
        dprintf(args.outfd, "[%s] Thread #%d: waiting for connection \n", timestamp(), *nth);
        xsem_wait(conr->handler_sem);

        if (is_finished())
        {
            xsem_post(conr->handler_sem);
            break;
        }

        xsem_wait(dynr->load_mutex);
        dynr->handler_count++;
        float load = dynr->handler_count / (float)(dynr->n - 1);
        xsem_post(dynr->load_mutex);

        if (need_resize(load))
        {
            xsem_wait(dynr->load_mutex);
            if (!dynr->resize)
            {
                dynr->resize = TRUE;

                xsem_post(dynr->pooler_sem); // wake up pooler.
            }
            xsem_post(dynr->load_mutex);
        }

        dprintf(args.outfd, "[%s] A connection has been delegated to thread id #%d, system load %.1f%%\n",
                timestamp(), *nth, 100 * get_load());

        xsem_wait(conr->graph_mutex);
        int clientfd = dequeue(conr->client_queue);
        xsem_post(conr->graph_mutex);

        // get the indexes.
        xread(clientfd, recv_packet, packet_len);
        struct Packet *indices = (struct Packet *)recv_packet;

        dprintf(args.outfd, "[%s] Thread #%d: searching database for a path from node %d to node %d\n",
                timestamp(), *nth, indices->i1, indices->i2);
        long in_cache = read_database(indices->i1, indices->i2);
        char *path;
        if (in_cache)
        {
            path = (char *)in_cache;
            dprintf(args.outfd, "[%s] Thread #%d: path found in database: %s\n",
                    timestamp(), *nth, path);
        }
        else
        {
            // find the path.
            dprintf(args.outfd, "[%s] Thread #%d: no path in database, calculating %d->%d\n",
                    timestamp(), *nth, indices->i1, indices->i2);
            xsem_wait(conr->graph_mutex);
            struct Queue *bfs = BFS(conr->graph, indices->i1, indices->i2);
            xsem_post(conr->graph_mutex);

            path = prepare_packet(bfs);

            if (bfs == NULL)
                dprintf(args.outfd, "[%s] Thread #%d: %s from node %d to %d.\n",
                        timestamp(), *nth, path, indices->i1, indices->i2);
            else
            {
                destroy_queue(bfs);
                dprintf(args.outfd, "[%s] Thread #%d: path calculated: %s\n",
                        timestamp(), *nth, path);
            }

            write_database(path, indices->i1, indices->i2);
            dprintf(args.outfd, "[%s] Thread #%d: responding to client and adding path to database\n",
                    timestamp(), *nth);
        }

        int len = strlen(path);
        xwrite(clientfd, path, len);
        close(clientfd);

        xsem_wait(dynr->load_mutex);
        dynr->handler_count--;
        xsem_post(dynr->load_mutex);
    }

    free(recv_packet);
    free(nth);
    pthread_exit(NULL);
    return NULL;
}

long read_database(int i, int j)
{
    // reader is entering the house.
    xsem_wait(conr->read_try);
    xsem_wait(conr->read_mutex);
    conr->read_count++;
    if (conr->read_count == 1)
        xsem_wait(conr->cache_mutex);
    xsem_post(conr->read_mutex);
    xsem_post(conr->read_try);

    /* start read */
    long path = get_cache(conr->cache, i, j);
    /* end read */

    // reader is leaving the house.
    xsem_wait(conr->read_mutex);
    conr->read_count--;
    if (conr->read_count == 0)
        xsem_post(conr->cache_mutex);
    xsem_post(conr->read_mutex);
    return path;
}

void write_database(char *path, int i, int j)
{
    // writer is entering the house.
    xsem_wait(conr->write_mutex);
    conr->write_count++;
    if (conr->write_count == 1)
        xsem_wait(conr->read_try);
    xsem_post(conr->write_mutex);
    xsem_wait(conr->cache_mutex);

    /* write start */
    to_cache(conr->cache, i, j, path);
    /* write end */

    xsem_post(conr->cache_mutex);
    // writer is leaving the house.
    xsem_wait(conr->write_mutex);
    conr->write_count--;
    if (conr->write_count == 0)
        xsem_post(conr->read_try);
    xsem_post(conr->write_mutex);
}

void init_shared_resources()
{
    conr = xmalloc(sizeof(struct ConnHandlerResource));
    dynr = xmalloc(sizeof(struct DynamicPoolerResource));

    conr->graph = NULL;
    conr->graph_mutex = xmalloc(sizeof(sem_t));
    conr->handler_sem = xmalloc(sizeof(sem_t));
    conr->client_queue = create_queue(1024);
    xsem_init(conr->handler_sem, 0);
    xsem_init(conr->graph_mutex, 1);
    conr->finished = FALSE;

    conr->cache = NULL;
    conr->read_try = xmalloc(sizeof(sem_t));
    conr->read_mutex = xmalloc(sizeof(sem_t));
    conr->write_mutex = xmalloc(sizeof(sem_t));
    conr->cache_mutex = xmalloc(sizeof(sem_t));
    conr->read_count = conr->write_count = 0;
    xsem_init(conr->read_try, 1);
    xsem_init(conr->read_mutex, 1);
    xsem_init(conr->write_mutex, 1);
    xsem_init(conr->cache_mutex, 1);

    dynr->pool = NULL;
    dynr->n = args.min_thread + 1; // first element is the pool resizer thread.
    dynr->pooler_sem = xmalloc(sizeof(sem_t));
    dynr->load_mutex = xmalloc(sizeof(sem_t));
    dynr->handler_count = 0;

    xsem_init(dynr->pooler_sem, 0);
    xsem_init(dynr->load_mutex, 1);

    dynr->main_thread = pthread_self();
}

void destroy_shared_resources()
{
    destroy_queue(conr->client_queue);
    xsem_destroy(conr->graph_mutex);
    xsem_destroy(conr->handler_sem);
    xsem_destroy(conr->read_try);
    xsem_destroy(conr->read_mutex);
    xsem_destroy(conr->write_mutex);
    xsem_destroy(conr->cache_mutex);
    free(conr->handler_sem);
    free(conr->graph_mutex);
    free(conr->client_queue);
    free(conr->read_try);
    free(conr->read_mutex);
    free(conr->write_mutex);
    free(conr->cache_mutex);
    if (conr->graph != NULL)
    {
        destroy_graph(conr->graph);
        free(conr->graph);
        conr->graph = NULL;
    }
    if (conr->cache != NULL)
    {
        destroy_cache(conr->cache);
        free(conr->cache);

        conr->cache = NULL;
    }

    xsem_destroy(dynr->pooler_sem);
    xsem_destroy(dynr->load_mutex);

    free(dynr->pooler_sem);
    free(dynr->load_mutex);
    if (dynr->pool != NULL)
    {
        free(dynr->pool);
        dynr->pool = NULL;
    }

    free(conr);
    free(dynr);
    xclose(args.outfd);
}

int digit(int n)
{
    int digit_number = 0;
    while (n != 0)
    {
        n = n / 10;
        ++digit_number;
    }
    return digit_number;
}

char *prepare_packet(struct Queue *bfs)
{
    int max_byte_per_node = digit(conr->graph->V) + 4; // <number> + "->".
    if (bfs == NULL)
    {
        char *packet = xmalloc(19);
        strcpy(packet, "path not possible.");
        return packet;
    }
    char *packet = xmalloc(max_byte_per_node * size(bfs));

    char *temp = xmalloc(max_byte_per_node);
    int offset = 0;
    while (!is_empty(bfs))
    {
        int node = dequeue(bfs);
        if (!is_empty(bfs))
            sprintf(temp, "%d->", node);
        else
            sprintf(temp, "%d.", node);
        sprintf(packet + offset, "%s", temp);
        offset += strlen(temp);
    }

    free(temp);
    return packet;
}

void create_pool()
{
    dynr->pool = xmalloc(sizeof(pthread_t) * dynr->n);
    for (int i = 1; i < dynr->n; i++)
    {
        int *nth = xmalloc(sizeof(int));
        *nth = i;
        xthread_create(&dynr->pool[i], connection_handler, nth);
    }

    xthread_create(&dynr->pool[0], pool_resizer, dynr);
    dprintf(args.outfd, "[%s] A pool of %d threads have been created\n",
            timestamp(), dynr->n - 1);
}

float get_load()
{
    sem_wait(dynr->load_mutex);
    float load = (float)dynr->handler_count / (dynr->n - 1);
    xsem_post(dynr->load_mutex);

    return load;
}

int need_resize(float load)
{
    if (load == 1)
        dprintf(args.outfd, "[%s] No thread is available! Waiting for one.\n", timestamp());
    return load >= MAX_LOAD;
}

void *pool_resizer(void *p)
{
    struct DynamicPoolerResource *r = (struct DynamicPoolerResource *)p;
    float grow_size = 1.25;

    while (TRUE)
    {
        xsem_wait(dynr->pooler_sem);

        // float load = get_load();
        if (r->n * grow_size <= args.max_thread)
        {
            xsem_wait(dynr->load_mutex);
            int old_n = r->n;
            r->n = (r->n - 1) * grow_size + 1;
            xsem_post(dynr->load_mutex);

            r->pool = xrealloc(r->pool, sizeof(pthread_t) * r->n);
            dprintf(args.outfd, "[%s] System load 75%%, pool extended to %d threads.\n", timestamp(), r->n - 1);

            // add threads to pool.
            for (int i = old_n; i < r->n; i++)
            {
                int *nth = xmalloc(sizeof(int));
                *nth = i;
                xthread_create(&r->pool[i], connection_handler, nth);
            }
        }
        xsem_wait(dynr->load_mutex);
        dynr->resize = FALSE;
        xsem_post(dynr->load_mutex);
    }

    pthread_exit(NULL);
    return NULL;
}

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <float.h>
#include <signal.h>
#include "utils.h"
#include "model.h"
#include "queue.h"

#define NEWLINE_DELIMETER "\n"


/* all resources needed to supervise the florists and manage sync */
struct FloristManager
{
    struct Florist **florist;
    struct Queue **queue_list;
    size_t count;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
    pthread_t *t;
    int *finished;
};

struct ClientManager
{
    struct Client **client;
    size_t count;
};

/* sync resources for each florist */
struct Sync
{
    struct Florist *florist;
    struct Queue *queue; /* each florist has it's own queue assigned */
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
    int *finished;
};

/* actors functions: main thread supervisor and infinite many florist threads */
void supervisor(int);
void *florist(void *);

int main(int argc, char *argv[])
{
    // multiple threads will be writing simultaneously to stdout.
    // better we see the outcome in the correct order.
    setvbuf(stdout, NULL, _IONBF, 0);

    // seed.
    srand(time(0));

    int fd;
    parse_args(argc, argv, &fd);
    supervisor(fd);
    return 0;
}

/* functions regarding to model the actors, constructor and destructors of model*/
void init_models(int fd, struct FloristManager *, struct ClientManager *);
void destroy_models(struct FloristManager *, struct ClientManager *);
void destroy_florist(struct FloristManager *);
void destroy_client(struct ClientManager *);

/* controllers */
void assign_florists(struct FloristManager *, struct ClientManager*);
size_t find_florist(struct FloristManager *, struct Client);
void print_stats(struct FloristManager *fm);

/* sync tools encapsulated with their errors handled */
void lock(pthread_mutex_t *);
void unlock(pthread_mutex_t *);
void cwait(pthread_cond_t *, pthread_mutex_t *);
void broadcast(pthread_cond_t *);

/* defined pointers to the resources so that interrupt handler can free them when needed */
struct FloristManager *global_fm = NULL;
struct ClientManager *global_cm = NULL;

// all resources used by all threads are freed.
void handle_sigint(int sig)
{
    printf("\nCaught signal %d. Terminating gracefully...\n", sig);
    if (global_fm != NULL)
    {
        free(global_fm->mutex);
        free(global_fm->cond);
        free(global_fm->finished);

        for (size_t i = 0; i < global_fm->count; i++)
        {
            destroy_queue(global_fm->queue_list[i]);
            free(global_fm->queue_list[i]);
            free(global_fm->florist[i]);
        }
        free(global_fm->florist);
        free(global_fm->queue_list);
        free(global_fm->t);
    }
    if (global_cm != NULL)
        destroy_client(global_cm);
    exit(EXIT_SUCCESS);
}

void supervisor(int fd)
{
    signal(SIGINT, handle_sigint);
    struct FloristManager fm;
    struct ClientManager cm;
    init_models(fd, &fm, &cm);

    assign_florists(&fm, &cm);

    /* processing requests */
    for (size_t i = 0; i < cm.count; i++)
    {
        size_t florist = find_florist(&fm, *cm.client[i]);
        lock(fm.mutex);
        enqueue(fm.queue_list[florist], cm.client[i]); // add the flower to queue of the florist.
        broadcast(fm.cond);                             // wake up florist.
        unlock(fm.mutex);
    }
    /* end of processing requests */


    // signal finished.
    lock(fm.mutex);
    *fm.finished = TRUE;
    broadcast(fm.cond);
    unlock(fm.mutex);


    for (size_t i = 0; i < fm.count; i++)
        xthread_join(fm.t[i]);
    printf("\nAll requests processed.");
        
    print_stats(&fm);

    destroy_models(&fm, &cm);
}

int read_raw_data(int fd, char **buf)
{
    int file_size = xlseek(fd, 0, SEEK_END); // learn the size of the file.
    *buf = (char *)xmalloc(file_size);

    xlseek(fd, 0, SEEK_SET); // rewind.
    if (xread(fd, *buf, file_size) != file_size)
        xerror(__func__, "file_size does not match!");
    return file_size;
}

int frequency(char *str, char c)
{
    int freq = 0;
    do
        if (c == *str)
            ++freq;
    while (*(++str) != '\0');

    return freq;
}

void read_single_florist(char *buf, struct Florist *florist)
{
    char *flowers = xmalloc(strlen(buf));
    sscanf(buf, "%s (%f,%f; %f) : %[^\t\n]", florist->name, &florist->location.x,
           &florist->location.y, &florist->speed, flowers);
    florist->flower_count = frequency(flowers, ',') + 1;
    florist->flower = xmalloc(florist->flower_count * sizeof(char*));
    char *save_ptr = flowers;
    for (size_t i = 0; i < florist->flower_count; i++)
    {
        char *token = strtok_r(save_ptr, ", ", &save_ptr);
        florist->flower[i] = xmalloc(NAME_LENGTH);
        strncpy(florist->flower[i], token, NAME_LENGTH);
    }
    florist->total_sale = florist->total_time = 0;
    free(flowers);
}

void read_single_client(char *buf, struct Client *client)
{
    char flower[NAME_LENGTH];
    sscanf(buf, "%s (%f,%f): %s", client->name, &client->location.x,
           &client->location.y, flower);
    strncpy(client->flower, flower, NAME_LENGTH);
}

void init_models(int fd, struct FloristManager *fm, struct ClientManager *cm)
{
    /* read data */
    char *buf;
    read_raw_data(fd, &buf);
    xclose(fd);

    /* initialize models */
    fm->florist = (struct Florist **)xmalloc(sizeof(struct Florist *));
    cm->client = (struct Client **)xmalloc(sizeof(struct Client *));
    cm->count = 0;
    fm->count = 0;

    /* first token */
    char *token = strtok(buf, NEWLINE_DELIMETER);

    /* walk through other tokens */
    while (token != NULL)
    {
        if (strchr(token, ';') != 0)
        {
            fm->count++;
            fm->florist = (struct Florist **)realloc(fm->florist, sizeof(struct Florist *) * fm->count);
            fm->florist[fm->count - 1] = (struct Florist *)xmalloc(sizeof(struct Florist));
            read_single_florist(token, fm->florist[fm->count - 1]);
        }
        else
        {
            cm->count++;
            cm->client = (struct Client **)realloc(cm->client, sizeof(struct Client *) * cm->count);
            cm->client[cm->count - 1] = (struct Client *)xmalloc(sizeof(struct Client));
            read_single_client(token, cm->client[cm->count - 1]);
        }

        token = strtok(NULL, NEWLINE_DELIMETER);
    }
    free(buf);
    global_cm = cm;
}

// several mutex contruction construction with error handling.
void init_mutex(pthread_mutex_t *mutex)
{
    pthread_mutexattr_t mutex_attr;
    if (pthread_mutexattr_init(&mutex_attr) != 0)
        xerror(__func__, "pthread_mutexattr_init");
    if (pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK) != 0)
        xerror(__func__, "pthread_mutexattr_settype");
    if (pthread_mutex_init(mutex, &mutex_attr) != 0)
        xerror(__func__, "pthread_mutex_init");
    if (pthread_mutexattr_destroy(&mutex_attr) != 0)
        xerror(__func__, "pthread_mutexattr_destroy");
}

void destroy_mutex(pthread_mutex_t *mutex)
{
    if (pthread_mutex_destroy(mutex))
        xerror(__func__, "pthread_mutex_destroy");
}

// several condition variable construction procedures with error handling.
void init_cond(pthread_cond_t *cond)
{
    pthread_condattr_t cond_attr;
    if (pthread_condattr_init(&cond_attr) != 0)
        xerror(__func__, "pthread_condattr_init");
    if (pthread_cond_init(cond, &cond_attr) != 0)
        xerror(__func__, "pthread_cond_init");
    if (pthread_condattr_destroy(&cond_attr) != 0)
        xerror(__func__, "pthread_condattr_destroy");
}

void destroy_cond(pthread_cond_t *cond)
{
    if (pthread_cond_destroy(cond) != 0)
        xerror(__func__, "pthread_cond_destroy");
}

void assign_florists(struct FloristManager *fm, struct ClientManager *cm)
{
    fm->mutex = (pthread_mutex_t *)xmalloc(sizeof(pthread_mutex_t));
    fm->cond = (pthread_cond_t *)xmalloc(sizeof(pthread_cond_t));
    init_mutex(fm->mutex);
    init_cond(fm->cond);

    fm->finished = (int *)xmalloc(sizeof(int));
    *(fm->finished) = FALSE;

    fm->queue_list = (struct Queue **)xmalloc(sizeof(struct Queue *) * fm->count);
    fm->t = (pthread_t *)xmalloc(sizeof(pthread_t) * fm->count);
    for (size_t i = 0; i < fm->count; i++)
    {
        fm->queue_list[i] = create_queue(cm->count);
        struct Sync *sync = (struct Sync *)xmalloc(sizeof(struct Sync));
        sync->florist = fm->florist[i];
        sync->queue = fm->queue_list[i];
        sync->mutex = fm->mutex;
        sync->cond = fm->cond;
        sync->finished = fm->finished;
        xthread_create(&fm->t[i], florist, sync);
    }
    printf("\n%lu florists have been created.", fm->count);
    global_fm = fm;
}

void destroy_florist(struct FloristManager *fm)
{
    destroy_mutex(fm->mutex);
    destroy_cond(fm->cond);
    free(fm->mutex);
    free(fm->cond);
    free(fm->finished);

    for (size_t i = 0; i < fm->count; i++)
    {
        destroy_queue(fm->queue_list[i]);
        free(fm->queue_list[i]);

        for (size_t j = 0; j < fm->florist[i]->flower_count; j++)
            free(fm->florist[i]->flower[j]);
        free(fm->florist[i]->flower);
        free(fm->florist[i]);
    }

    free(fm->florist);
    free(fm->queue_list);
    free(fm->t);
}

void destroy_client(struct ClientManager *cm)
{
    for (size_t i = 0; i < cm->count; i++)
        free(cm->client[i]);
    free(cm->client);
}

void destroy_models(struct FloristManager *fm, struct ClientManager *cm)
{
    destroy_florist(fm);
    destroy_client(cm);
    global_fm = NULL;
    global_cm = NULL;
}

void lock(pthread_mutex_t *mutex)
{
    if (pthread_mutex_lock(mutex) != 0)
        xerror(__func__, "pthread_mutex_lock");
}

void unlock(pthread_mutex_t *mutex)
{
    if (pthread_mutex_unlock(mutex) != 0)
        xerror(__func__, "pthread_mutex_unlock");
}

void cwait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    if (pthread_cond_wait(cond, mutex) != 0)
        xerror(__func__, "pthread_cond_wait");
}

void broadcast(pthread_cond_t *cond)
{
    if (pthread_cond_broadcast(cond) != 0)
        xerror(__func__, "pthread_cond_broadcast");
}

int random_time()
{
    return (rand() % 250) + 1;
}

float distance(struct Florist *florist, struct Client *client)
{
    return fabs(florist->location.x - client->location.x) +
           fabs(florist->location.y - client->location.y);
}

void *florist(void *p)
{
    struct Sync *sync = (struct Sync *)p;
    for (;;)
    {
        lock(sync->mutex);
        while (is_empty(sync->queue) && !(*sync->finished))
            cwait(sync->cond, sync->mutex);
        if (*sync->finished && is_empty(sync->queue))
        {
            unlock(sync->mutex);
            break;
        }
        struct Client *client = dequeue(sync->queue);
        unlock(sync->mutex);
        int prep_time = random_time() + sync->florist->speed * distance(sync->florist, client);
        usleep(prep_time);
        sync->florist->total_time += prep_time;
        sync->florist->total_sale++;
        printf("\nFlorist %s has delivered a %s to %s in %dms", sync->florist->name,
               client->flower, client->name, prep_time);
    }
    printf("\n%s closing shop.", sync->florist->name);
    free(sync);
    pthread_exit(NULL);
    return NULL;
}

float max(float n1, float n2)
{
    return (n1 > n2) ? n1 : n2;
}

float chebyshev_distance(struct Point p1, struct Point p2)
{
    return max(fabs(p1.x - p2.x), fabs(p1.y - p2.y));
}

int florist_delivers(struct Florist *florist, char *flower)
{
    int delivers = FALSE;
    for (size_t i = 0; i < florist->flower_count; i++)
        delivers |= (strncmp(florist->flower[i], flower, NAME_LENGTH) == 0);
    return delivers;
}

size_t find_florist(struct FloristManager *fm, struct Client client)
{
    size_t florist_index = fm->count;
    float min = FLT_MAX;
    for (size_t i = 0; i < fm->count; i++)
    {
        if (florist_delivers(fm->florist[i], client.flower))
        {
            float distance = chebyshev_distance(fm->florist[i]->location, client.location);
            if (distance <= min)
            {
                min = distance;
                florist_index = i;
            }
        }
    }
    return florist_index;
}

void print_stats(struct FloristManager *fm)
{
    printf("\nSale statistics for today:");
    printf("\n\n-------------------------------------------------\n");
    printf("Florist\t\t # of sales\t\t Total time\n");
    printf("-------------------------------------------------\n");

    for (size_t i = 0; i < fm->count; i++)
        printf("%s\t\t %3d \t\t\t %dms\n", fm->florist[i]->name, fm->florist[i]->total_sale, fm->florist[i]->total_time);
    printf("-------------------------------------------------\n");
}
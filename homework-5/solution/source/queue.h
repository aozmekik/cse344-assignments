#ifndef QUEUE_H
#define QUEUE_H

#include "utils.h"
#include "model.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* Simple Queue Implementation. 
 * represents a simple queue implementation specialized for clients.
 * clients waiting for flovers will be realized as a queue. 
 * @see program.c
 **/

struct Queue
{
    int front, back;
    unsigned int cap, size;
    struct Client **line;
};

struct Queue *create_queue(unsigned int _cap)
{
    struct Queue *q = (struct Queue *)xmalloc(sizeof(struct Queue));
    q->cap = _cap;
    q->front = q->size = 0;
    q->back = q->cap - 1;
    q->line = (struct Client **)xmalloc(q->cap * sizeof(struct Client *));
    return q;
}

int is_full(struct Queue *q)
{
    return (q->size == q->cap);
}

int is_empty(struct Queue *q)
{
    return (q->size == 0);
}

void enqueue(struct Queue *q, struct Client *client)
{
    if (is_full(q))
        xerror(__func__, "queue is full!");
    q->back = (q->back + 1) % q->cap; // circular array.
    q->line[q->back] = client;
    q->size++;
}

struct Client *dequeue(struct Queue *q)
{
    if (is_empty(q))
        xerror(__func__, "queue is empty!");
    struct Client *client = q->line[q->front];
    // printf("\n%s", client->name);
    q->front = (q->front + 1) % q->cap;
    q->size--;
    return client;
}

struct Client *front(struct Queue *q)
{
    if (is_empty(q))
        xerror(__func__, "queue is empty!");
    return q->line[q->front];
}

void destroy_queue(struct Queue *q)
{
    free(q->line);
}

#endif
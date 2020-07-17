#ifndef QUEUE_H
#define QUEUE_H

#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/** Simple Queue Implementation. **/

struct Queue
{
    int front, back;
    unsigned int cap, size;
    long *line;
};

struct Queue *create_queue(unsigned int _cap)
{
    struct Queue *q = (struct Queue *)xmalloc(sizeof(struct Queue));
    q->cap = _cap;
    q->front = q->size = 0;
    q->back = q->cap - 1;
    q->line = (long *)xmalloc(q->cap * sizeof(long));
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

void enlarge(struct Queue **queue);

void enqueue(struct Queue **q, long item)
{
    if (is_full(*q))
        enlarge(q);
    (*q)->back = ((*q)->back + 1) % (*q)->cap; // circular array.
    (*q)->line[(*q)->back] = item;
    (*q)->size++;
}

long dequeue(struct Queue *q)
{
    if (is_empty(q))
        xerror(__func__, "queue is empty!");
    long item = q->line[q->front];
    q->front = (q->front + 1) % q->cap;
    q->size--;
    return item;
}

long front(struct Queue *q)
{
    if (is_empty(q))
        xerror(__func__, "queue is empty!");
    return q->line[q->front];
}

long back(struct Queue *q)
{
    if (is_empty(q))
        xerror(__func__, "queue is empty!");
    return q->line[q->back];
}

void destroy_queue(struct Queue *q)
{
    free(q->line);
}

int size(struct Queue *q)
{
    return q->size;
}

void copy(struct Queue *src, struct Queue *dest)
{
    dest->back = src->back;
    dest->cap = src->cap;
    dest->front = src->front;
    dest->size = src->size;
    for (unsigned int i = 0; i < src->cap; i++)
        dest->line[i] = src->line[i];
}


void enlarge(struct Queue **queue)
{
    struct Queue *new_queue = create_queue((*queue)->cap * 2);
    while (!is_empty(*queue))
        enqueue(&new_queue, dequeue(*queue));
    destroy_queue(*queue);
    *queue = new_queue;
}


#endif
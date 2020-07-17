#ifndef CACHE_H
#define CACHE_H
#include "utils.h"

struct CacheNode
{
    int vertex;
    char *path;
    struct CacheNode *next;
};

struct Cache
{
    int V;
    struct CacheNode **list;
};

struct CacheNode *create_cache_node(int v, char *path)
{
    struct CacheNode *node = (struct CacheNode *)xmalloc(sizeof(struct CacheNode));
    node->vertex = v;
    node->next = NULL;
    node->path = path;
    return node;
}

struct Cache *create_cache(int V)
{
    struct Cache *cache = (struct Cache *)xmalloc(sizeof(struct Cache));
    cache->V = V;
    cache->list = (struct CacheNode **)xmalloc(V * sizeof(struct CacheNode *));

    for (int i = 0; i < V; i++)
        cache->list[i] = NULL;

    return cache;
}

void to_cache(struct Cache *cache, int i, int j, char *path)
{
    struct CacheNode *node = create_cache_node(j, path);

    /* connect new node to rest of the neighbours */
    node->next = cache->list[i];
    cache->list[i] = node;
}

long get_cache(struct Cache *cache, int i, int j)
{
    struct CacheNode *node = cache->list[i];
    while (node != NULL)
    {
        if (node->vertex == j)
            return (long)node->path;
        node = node->next;
    }

    return FALSE;
}

void delete_cache_node(struct CacheNode *node)
{
    while (node != NULL)
    {
        delete_cache_node(node->next);
        free(node->path);
        free(node);
    }
}

void destroy_cache(struct Cache *cache)
{
    for (int i = 0; i < cache->V; i++)
        delete_cache_node(cache->list[i]);
    free(cache->list);
}

#endif
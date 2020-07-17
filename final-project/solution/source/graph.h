
#ifndef GRAPH_H
#define GRAPH_H
#include "utils.h"
#include "queue.h"

/**
 * graph.h
 * represent the graph implementation with adjacency list.
 * @see server.c
 **/

struct AdjacencyNode
{
    int vertex;
    struct AdjacencyNode *next;
};

struct Graph
{
    int V;
    struct AdjacencyNode **list;
    int *visited;
};

struct AdjacencyNode *create_graph_node(int v)
{
    struct AdjacencyNode *node = (struct AdjacencyNode *)xmalloc(sizeof(struct AdjacencyNode));
    node->vertex = v;
    node->next = NULL;
    return node;
}

struct Graph *create_graph(int V)
{
    struct Graph *graph = (struct Graph *)xmalloc(sizeof(struct Graph));
    graph->V = V;
    graph->list = (struct AdjacencyNode **)xmalloc(V * sizeof(struct AdjacencyNode *));
    graph->visited = (int *)xmalloc(sizeof(int) * V);

    for (int i = 0; i < V; i++)
    {
        graph->list[i] = NULL;
        graph->visited[i] = 0;
    }

    return graph;
}

void add_edge(struct Graph *graph, int i, int j)
{
    struct AdjacencyNode *node = create_graph_node(j);

    /* connect new node to rest of the neighbours */
    node->next = graph->list[i];
    graph->list[i] = node;
}

void delete_graph_node(struct AdjacencyNode *node)
{
    while (node != NULL)
    {
        delete_graph_node(node->next);
        free(node);
    }
}

void destroy_graph(struct Graph *graph)
{
    for (int i = 0; i < graph->V; i++)
        delete_graph_node(graph->list[i]);
    free(graph->visited);
    free(graph->list);
}

int edge(struct Graph *graph, int i, int j)
{
    struct AdjacencyNode *node = graph->list[i];
    while (node != NULL)
    {
        if (node->vertex == j)
            return TRUE;
        node = node->next;
    }
    return FALSE;
}

struct Queue *BFS(struct Graph *graph, int start, int end)
{
    if (start == end) // source and dest given as the same.
    {
        struct Queue *result = create_queue(graph->V);
        enqueue(&result, start);
        return result;
    }
    if (edge(graph, start, end)) // there is an edge from source to dest.
    {
        struct Queue *result = create_queue(graph->V);
        enqueue(&result, start);
        enqueue(&result, end);
        return result;
    }

    /* finding path with bfs algorithm */
    struct Queue *paths = create_queue(1024);
    struct Queue *path = create_queue(16);
    int *visited = (int* )xmalloc(sizeof(int) * graph->V);
    for (int i = 0; i < graph->V; i++)
        visited[i] = FALSE;

    enqueue(&path, start);
    enqueue(&paths, (long)path);

    int found = FALSE;
    while (!is_empty(paths))
    {
        path = (struct Queue *)dequeue(paths);
        int node = back(path);
        if (node == end)
        {
            found = TRUE;
            break;
        }

        struct AdjacencyNode *adj = graph->list[node];
        visited[node] = TRUE;
        while (adj != NULL)
        {
            if (visited[adj->vertex])
                adj = adj->next;
            else
            {
                struct Queue *new_path = create_queue(16);
                copy(path, new_path);
                enqueue(&new_path, adj->vertex);
                enqueue(&paths, (long)new_path);
                adj = adj->next;
            }
        }

        destroy_queue(path);
        free(path);
    }

    // clean up resources.
    while (!is_empty(paths))
    {
        struct Queue *p = (struct Queue *)dequeue(paths);
        destroy_queue(p);
        free(p);
    }
    destroy_queue(paths);
    free(paths);

    return found ? path : NULL;
}

#endif
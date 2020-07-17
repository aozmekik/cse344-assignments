#ifndef MODEL_H
#define MODEL_H

#define NAME_LENGTH 32

/***
 * MODELS HEADER FILE REPRESENTING DATA
 * represents the model objects to represents the actors in the homework.
 * @see program.c
 ***/

// /* defining the 5 types of flowers */
// enum Flower
// {
//     ORCHID,
//     CLOVE,
//     ROSE,
//     VIOLET,
//     DAFFODIL,
// };

struct Point // represents a point in a cartesian coordinat system
{
    float x, y;
};

struct Client
{
    char name[NAME_LENGTH];
    struct Point location;
    char flower[NAME_LENGTH]; // client demands 1 flower.
};

struct Florist
{
    char name[NAME_LENGTH];
    struct Point location;
    float speed;
    char **flower; // florist delivers n kind of flowers.
    int total_sale, total_time;
    unsigned int flower_count;
};

#endif
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "utils.h"

#define CHEF_NUMBER 6
#define INGREDIENT_NUMBER 4

#define M 'M' // their literals could be changed.
#define F 'F'
#define W 'W'
#define S 'S'

/** DATA MODEL STRUCTURE **/

// we will use bitwise mask to represent ingredients.
enum Ingredient
{
    MILK = 2,
    FLOUR = 4,
    WALNUTS = 8,
    SUGAR = 16,
    EMPTY,
};

/** THREAD & SYNC STRUCTURE **/
struct Resources
{
    int index;
    int *finished; // to indicate supplier is finished delivery.
    int sem_ingred;
    sem_t *dessert; // chefs will be posting desert.
    sem_t *finish_mutex;
    int *store; // a dummy data structure to simulate storing ingredients delivered by wholesaler.
    sem_t *store_mutex;
};

/** CONSTANTS */
const char INGS[][8] = {"milk", "flour", "walnuts", "sugar"};
const int DISCRETE_MASKS[CHEF_NUMBER] = {
    MILK | FLOUR,
    MILK | WALNUTS,
    MILK | SUGAR,
    FLOUR | WALNUTS,
    FLOUR | SUGAR,
    WALNUTS | SUGAR,
};

// actors.
void *chef(void *arg);
void wholesaler(int fd, struct Resources *ws);

void init(struct Resources *r);
void destroy(struct Resources *r);
void copy(struct Resources *dest, struct Resources *src);

// wholesaler will be represented as main thread.
int main(int argc, char *argv[])
{
    // multiple threads will be writing simultaneously to stdout.
    // better we see the outcome in the correct order.
    setvbuf(stdout, NULL, _IONBF, 0);
    srand(time(0));

    int fd = 0;
    struct Resources r;
    pthread_t tid[CHEF_NUMBER];

    init(&r);
    parse_args(argc, argv, &fd);

    struct Resources ri[CHEF_NUMBER];

    for (int i = 0; i < CHEF_NUMBER; i++) // create chefs.
    {
        ri[i] = r;
        ri[i].index = i;
        xthread_create(&tid[i], chef, &ri[i]);
    }

    wholesaler(fd, &r);

    for (int i = 0; i < CHEF_NUMBER; i++) // clean chefs.
        xthread_join(tid[i]);

    destroy(&r);
    printf("\nSimulation finished!\n");
    return 0;
}

void init(struct Resources *r)
{
    // getting the shared resources from to heap.

    r->sem_ingred = systemV_semcreate();
    systemV_seminit(r->sem_ingred); // init all four semaphore with 0.
    r->finished = (int *)xmalloc(sizeof(int));
    r->store = (int *)xmalloc(sizeof(int));
    r->dessert = (sem_t *)xmalloc(sizeof(sem_t));
    r->finish_mutex = (sem_t *)xmalloc(sizeof(sem_t));
    r->store_mutex = (sem_t *) xmalloc(sizeof(sem_t));
    *r->finished = FALSE;
    xsem_init(r->finish_mutex, 1);
    xsem_init(r->dessert, 0);
    xsem_init(r->store_mutex, 1);
}

void destroy(struct Resources *r)
{
    systemV_semdestroy(r->sem_ingred);
    xsem_destroy(r->dessert);
    xsem_destroy(r->finish_mutex);
    xsem_destroy(r->store_mutex);
    free(r->finished);
    free(r->dessert);
    free(r->finish_mutex);
    free(r->store);
    free(r->store_mutex);
}

// for the chef in the given index, it returns the mask indicating the ingredients of that chef.
int get_mask(int index)
{
    return DISCRETE_MASKS[index];
}

int mask_has(int mask, int ingredient)
{
    return mask & ingredient;
}

int ingredient(int n)
{
    int result = 2;
    for (int i = 0; i < n; i++)
        result <<= 1;
    return result;
}

// returns the appropriate string representing the supplies of the chef lacks/has.
char *print_mask(int mask, char str[])
{
    str[0] = '\0';

    int prev = FALSE;
    for (int i = 0; i < INGREDIENT_NUMBER; ++i)
    {
        // if ((!reverse && mask_has(mask, ingredient(i))) || (reverse && !mask_has(mask, ingredient(i))))
        if (mask_has(mask, ingredient(i)))
        {
            if (prev)
                strcat(str, " and ");
            strcat(str, INGS[i]);
            prev = TRUE;
        }
    }
    return str;
}

void take_delivery(struct Resources *ct, int n)
{
    for (int i = 0; i < INGREDIENT_NUMBER; i++)
        if (mask_has(get_mask(n), ingredient(i)))
            printf("\nchef%d has taken the %s", n, INGS[i]);

    xsem_wait(ct->store_mutex);
    *ct->store = EMPTY;
    xsem_post(ct->store_mutex);
}

void prepare_dessert(int n)
{
    int time = (rand() % 5) + 1;
    printf("\nchef%d is preparing the dessert (preparation time: %ds)", n, time);
    sleep(time);
    printf("\nchef%d has delivered the dessert to the wholesaler", n);
}

int finished(struct Resources *ct)
{
    int f = FALSE;
    xsem_wait(ct->finish_mutex);
    f = *ct->finished;
    xsem_post(ct->finish_mutex);
    return f;
}

// gets the semaphore index of ingredient mask contains.
void sem_of_ingredients(int mask, int index[2])
{
    int j = 0;
    for (int i = 0; i < INGREDIENT_NUMBER; i++)
        if (mask_has(mask, ingredient(i)))
            index[j++] = i;
    assert(j == 2);
}

void *chef(void *arg)
{
    struct Resources *ct = (struct Resources *)arg;
    int i = ct->index;
    int mask = get_mask(i);

    int sem_index[2];
    sem_of_ingredients(mask, sem_index);

    while (!finished(ct))
    {
        char str[32];
        printf("\nchef%d is waiting for %s", i, print_mask(mask, str));

        systemV_semwait(ct->sem_ingred, sem_index); // grab 2 ingredients.
        if (finished(ct))
            break;
        take_delivery(ct, i);
        prepare_dessert(i);
        sem_post(ct->dessert); // dessert is ready.
    }
    printf("\nchef%d exiting.", i);

    // wake up all chef waiting in vain.
    for (int i = 0; i < CHEF_NUMBER; i++)
    {
        mask = get_mask(i);
        sem_of_ingredients(mask, sem_index);
        systemV_sempost(ct->sem_ingred, sem_index);
    }
    pthread_exit(NULL);
    return NULL;
}

int next_supply(int fd)
{
    char buf[3]; // two chars and a newline.
    if (xread(fd, buf, 3) != 3)
        return FALSE;

    int mask = 0;
    for (int i = 0; i < 2; i++)
    {
        switch (buf[i])
        {
        case M:
            mask |= MILK;
            break;
        case F:
            mask |= FLOUR;
            break;
        case S:
            mask |= SUGAR;
            break;
        case W:
            mask |= WALNUTS;
            break;
        default:
            assert(FALSE);
        }
    }
    return mask;
}

void deliver(struct Resources *ws, int mask)
{
    xsem_wait(ws->store_mutex);
    *ws->store = mask;
    xsem_post(ws->store_mutex);
}

void wholesaler(int fd, struct Resources *ws)
{
    int mask = 0;
    int sem_index[2];
    while ((mask = next_supply(fd))) // get the next supply.
    {
        char str[32]; // temp string to store string of mask.
        // deliver supplies.
        printf("\nthe wholesaler delivers %s", print_mask(mask, str));
        sem_of_ingredients(mask, sem_index);
        deliver(ws, mask);
        systemV_sempost(ws->sem_ingred, sem_index); // post two ingredients.

        // wait for dessert.
        printf("\nthe wholesaler is waiting for dessert");
        xsem_wait(ws->dessert);
        printf("\nthe wholesaler has obtained the dessert and left to sell it");
    }

    // indicate that delivery is finished.
    xsem_wait(ws->finish_mutex);
    *ws->finished = TRUE;
    xsem_post(ws->finish_mutex);

    systemV_sempost(ws->sem_ingred, sem_index); // signal finish.
}
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <signal.h>


#define FOOD_KIND 3
#define SOUP 'P'
#define COURSE 'C'
#define DESERT 'D'

enum plate
{
    P, // soup
    C, // main course
    D, // desert.
    EMPTY,
};

struct ipc_key
{
    char room[16];      // room key.
    char semaphore[16]; // semaphores key.
};

struct ipc_item
{
    int *room;
    int *peek;
    sem_t *room_mutex;
    sem_t *peek_mutex;
    sem_t *empty; // empty slot in room.
    sem_t *full;  // full slot in room.
};

struct ipc_shared_mem
{
    int peek;
    sem_t room_mutex;
    sem_t peek_mutex;
    sem_t empty[FOOD_KIND];
    sem_t full[FOOD_KIND];
};

struct shared_mem_options
{
    char name[16];
    int flags;
    int fd;
    int prot;     // protections.
    mode_t perms; // permisions.
    size_t size;
};

// (rooms and semaphores) shared memory.
void *get_shared(char *key);
void remove_shared(char *key);
void create_room(char *key, int size);
void create_semaphore(char *key);

// sync.
void swait(sem_t *sem);
void spost(sem_t *sem);

// actors.
void supplier();
void cook(int nth);
void student(int nth, int graduate);
void supervisor(); // please see the report.

struct ipc_item ipc_open(struct ipc_key key);

const char FOODS[][16] = {"soup", "main course", "desert", "0"};

struct Args args;


// shared memory names.
const struct ipc_key KITCHEN = {"k-room", "k-semaphore"};
const struct ipc_key COUNTER = {"c-room", "c-semaphore"};
const struct ipc_key TABLE = {"t-room", "t-semaphore"};
const struct ipc_key SUPERVISOR = {"s-room", "s-semaphore"};

void init_rooms();
void destroy_rooms();

void gen_supply();

// gracefully exiting..
void int_handler(int sig){
    printf("\n[ ALL ] CTRL + C (%d). Exiting...", sig);
    destroy_rooms();
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
    signal (SIGINT, int_handler); 
    parse_args(argc, argv, &args);
    // multiple processes will be writing simultaneously to stdout.
    // better we see the outcome in the correct order.
    setvbuf(stdout, NULL, _IONBF, 0);
    srand(time(NULL));
    init_rooms();

    if (fork() == 0) // supplier. 
        supplier();
    else
    { // cooks.
        for (int i = 0; i < args.N; ++i)
            if (fork() == 0)
                cook(i);
    }

    if (fork() == 0)
        supervisor();

    // students.
    for (int i = 0; i < args.U; ++i) // graduates
        if (fork() == 0)
            student(i, FALSE);
    for (int i = 0; i < args.G; ++i) // undergraduates
        if (fork() == 0)
            student(i + args.U, TRUE);

    int status;
    while (wait(&status) > 0)
        ; // wait for all actors to finish.
    destroy_rooms();
    printf("\nServing Simulation is Done!");

    return 0;
}

void init(struct ipc_key key, int room_size, int sem_size, int dividor)
{
    // create.
    create_room(key.room, room_size * sizeof(enum plate));
    create_semaphore(key.semaphore);

    // initialize room.
    int *room = get_shared(key.room);
    for (int i = 0; i < room_size; ++i)
        room[i] = EMPTY;

    // initialize semaphores.
    struct ipc_shared_mem *ipc = (struct ipc_shared_mem *)get_shared(key.semaphore);
    xsem_init(&ipc->room_mutex, 1);
    xsem_init(&ipc->peek_mutex, 1);
    if (dividor)
    {
        int rem = sem_size % dividor;
        for (int i = 0; i < rem; ++i)
            xsem_init(&ipc->empty[i], (sem_size / dividor) + 1);
        for (int i = rem; i < FOOD_KIND; ++i)
            xsem_init(&ipc->empty[i], sem_size / dividor);

        for (int i = 0; i < FOOD_KIND; ++i)
            xsem_init(&ipc->full[i], 0);
    }
    else
    {
        for (int i = 0; i < FOOD_KIND; ++i)
        {
            xsem_init(&ipc->empty[i], sem_size);
            xsem_init(&ipc->full[i], 0);
        }
    }
    ipc->peek = args.L * args.M * FOOD_KIND; // LxM plates of each type.
}

// creates all needed shared memories.
void init_rooms()
{
    init(KITCHEN, args.K, args.K, FALSE);
    init(COUNTER, args.S, args.S, FOOD_KIND);
    init(TABLE, args.T, args.T, FALSE);
    init(SUPERVISOR, 1, 0, FALSE);

    struct ipc_item s = ipc_open(SUPERVISOR);

    *s.peek -= args.L * args.M * FOOD_KIND;
    *s.room = 0;

}

void destroy(struct ipc_key key)
{
    remove_shared(key.room);
    remove_shared(key.semaphore);
}

// destroy all resources.
void destroy_rooms()
{
    destroy(KITCHEN);
    destroy(COUNTER);
    destroy(TABLE);
    destroy(SUPERVISOR);
}

// returns a string containing number of each item in the room.
char *total_str(void *room, int size, char *str)
{
    int *r = (int *)room;
    int p = 0, c = 0, d = 0;
    for (int i = 0; i < size; ++i, ++r)
    {
        switch (*r) // count all 3 foods.
        {
        case P:
            ++p;
            break;
        case C:
            ++c;
            break;
        case D:
            ++d;
            break;
        default:
            break;
        }
    }
    sprintf(str, "P:%d,C:%d,D:%d=%d", p, c, d, p + c + d);
    return str;
}

// returns the total non-empty items in the room.
int non_empty(void *room, int size)
{
    int *p = (int *)room;
    int total = 0;
    for (int i = 0; i < size; ++i, ++p)
    {
        if (*p != EMPTY)
            ++total;
    }
    return total;
}

void put_item(void *room, int slot, int p)
{
    ((int *)room)[slot] = p;
}

int get_item(void *room, int slot)
{
    int item = ((int *)room)[slot];
    ((int *)room)[slot] = EMPTY;
    return item;
}

int next(void *room, int size, int item)
{
    int *p = (int *)room;
    for (int i = 0; i < size; ++i, ++p)
        if (*p == item)
            return i;
    return -1;
}

int item_number(void *room, int size, int item)
{
    int *p = (int *)room;
    int total = 0;
    for (int i = 0; i < size; ++i, ++p)
        if (*p == item)
            ++total;
    return total;
}

void get_supply(char *supply);
enum plate get_food(char *supply, int idx)
{
    switch (supply[idx])
    {
    case SOUP:
        return P;
    case COURSE:
        return C;
    case DESERT:
        return D;
    default:
        assert(FALSE);
    }
}

void supplier()
{
    // get resources.
    int deliver = args.L * args.M * FOOD_KIND; // LxM plates of each type.
    struct ipc_item k = ipc_open(KITCHEN);

    char *supply = xmalloc(deliver);
    get_supply(supply);

    // supply. (uniformly from each kind)
    for (int i = 0, slot = 0; i < deliver; ++i)
    {
        char str[32];
        enum plate food = get_food(supply, i);
        printf("\nThe supplier is going to the kitchen to deliver %s: kitchen items %s",
               FOODS[food], total_str(k.room, args.K, str));
        // kitchen room entrance
        swait(k.empty);
        swait(k.room_mutex);

        slot = next(k.room, args.K, EMPTY); // give next empty slot.
        assert(slot != -1);

        put_item(k.room, slot, food);
        printf("\nThe supplier delivered %s - after delivery: kitchen items %s", FOODS[food], total_str(k.room, args.K, str));
        spost(k.room_mutex);
        spost(&k.full[food]);
        // kitchen room exit.
        if (slot == args.K)
            slot = 0;
        // --food_delivered[food];
    }

    printf("\nSupplier finished supplying - GOODBYE! - # of items at kitchen: %d", non_empty(k.room, args.K));
    exit(EXIT_SUCCESS);
}

int finished(sem_t *sem, int *remaining)
{
    int fin = FALSE;
    swait(sem);
    fin = (*remaining == 0);
    spost(sem);
    return fin;
}

void decr(sem_t *sem, int *remaining)
{
    swait(sem);
    --(*remaining);
    spost(sem);
}

// gets the shared memory to an ipc item.
struct ipc_item ipc_open(struct ipc_key key)
{
    struct ipc_item ipc;

    // get room.
    ipc.room = (int *)get_shared(key.room);

    // get semaphores
    struct ipc_shared_mem *shared = (struct ipc_shared_mem *)get_shared(key.semaphore);
    ipc.peek = &shared->peek;
    ipc.room_mutex = &shared->room_mutex;
    ipc.peek_mutex = &shared->peek_mutex;
    ipc.empty = shared->empty;
    ipc.full = shared->full;
    return ipc;
}

void cook(int nth)
{
    int deliver = args.L * args.M; // LxM plates of each type.
    // get resources.
    // shared resources between children cooks and the supplier
    struct ipc_item k = ipc_open(KITCHEN); // kitchen communication.
    struct ipc_item c = ipc_open(COUNTER); // counter communication.

    int food_delivered[FOOD_KIND] = {deliver, deliver, deliver};


    // take supplies. (uniformly from each kind)
    int slot = 0;
    enum plate food = P; // MAKE THIS RANDOM.
    while (TRUE)
    {
        char str[32]; // to print room items.
        // peek the remaining delivery.
        if (finished(k.peek_mutex, k.peek))
            break;

        // peek the needed for the student.
        swait(c.room_mutex);
        swait(c.peek_mutex);
        if (*c.peek >= FOOD_KIND)
            *c.peek = P;
        food = *c.peek;
        ++*c.peek;
        spost(c.peek_mutex);
        spost(c.room_mutex);

        if (!food_delivered[food])
            break;

        printf("\nCook %d is going to the kitchen to wait for/get a plate - kitchen items: %s",
               nth, total_str(k.room, args.K, str));
        // kitchen room entrance
        swait(&k.full[food]);
        swait(k.room_mutex);

        slot = next(k.room, args.K, food); // give next looking food.
        if (slot == -1)
        {
            spost(k.room_mutex);
            spost(&k.full[food]);
            continue;
        }

        get_item(k.room, slot);
        decr(k.peek_mutex, k.peek);
        spost(k.room_mutex);
        spost(k.empty);
        // kitchen room exit.

        // counter room entrance
        swait(&c.empty[food]);
        swait(c.room_mutex);
        slot = next(c.room, args.S, EMPTY);
        put_item(c.room, slot, food);
        --food_delivered[food];
        spost(c.room_mutex);
        spost(&c.full[food]);
    }

    for (int i = 0; i < FOOD_KIND; ++i)
        spost(&k.full[i]); // signal other cooks.
    printf("\nCook %d finished serving - items at kitchen: %d – going home – GOODBYE!!!", nth, non_empty(k.room, args.K));
    exit(EXIT_SUCCESS);
}


void student(int nth, int graduate)
{
    // get resources.
    // shared resources between students and cooks
    struct ipc_item c = ipc_open(COUNTER); // counter communication.
    struct ipc_item t = ipc_open(TABLE);
    struct ipc_item s = ipc_open(SUPERVISOR);

    for (int i = 0; i < args.L; ++i)
    {
        char str[16];

        printf("\nStudent %d is going to the counter (round %d) - # of students at counter: %d and counter items %s",
        nth, i, *s.room, total_str(c.room, args.S, str));
        swait(s.room_mutex);
        ++*s.room;
        spost(s.room_mutex); 
        swait(s.peek_mutex);
        if (graduate)
            ++*s.peek;
        spost(s.peek_mutex);
        spost(s.empty); // student on the line.
        swait(&s.full[graduate]);
        swait(c.room_mutex);
        for (enum plate p = P; p < FOOD_KIND; ++p)
        {
            int slot = next(c.room, args.S, p);
            assert(slot != -1);
            get_item(c.room, slot);
            spost(&c.empty[p]);
        }
        spost(c.room_mutex);
        // counter room exit.
        decr(s.room_mutex, s.room);

        printf("\nStudent %d got food and is going to get a table (round %d) - # of empty tables :%d",
               nth, i, item_number(t.room, args.T, EMPTY));
        // look for table.
        swait(t.empty);
        swait(t.room_mutex);
        int slot = next(t.room, args.T, EMPTY); // next empty table.
        put_item(t.room, slot, TRUE);           // sit down and eat.
        printf("\nStudent %d sat at table %d to eat (round %d) - empty tables:%d",
               nth, slot, i, item_number(t.room, args.T, EMPTY));
        put_item(t.room, slot, EMPTY); // stand up.
        spost(t.room_mutex);
        spost(t.empty); // leave the table.
        printf("\nStudent %d left table %d to eat again (round %d) - empty tables:%d",
               nth, slot, i, item_number(t.room, args.T, EMPTY));
    }

    printf("\nStudent %d is done eating L=%d times – going home – GOODBYE!!!", nth, args.L);
    exit(EXIT_SUCCESS);
}

// supervises the graduate and undergraduate students.
void supervisor()
{
    struct ipc_item c = ipc_open(COUNTER);
    struct ipc_item s = ipc_open(SUPERVISOR);
    int deliver = args.L * args.M; // LxM plates of each type.

    for (int i = 0; i < deliver; ++i) // delivering 3 in a row.
    {
        printf("\nSupervisor waiting for counter to be ready.");
        for (int i = 0; i < FOOD_KIND; ++i) // wait for all kinds of foods to be ready.
            swait(&c.full[i]);
        printf("\nSupervisor got foods. Serving...");
        swait(s.empty); // if someone, any student is waiting.
        swait(s.peek_mutex);
        int graduate = (*s.peek) ? TRUE : FALSE; // if any graduate is waiting on the line.
        if (graduate)
            --*s.peek;
        spost(s.peek_mutex);
        spost(&s.full[graduate]);
    }
    printf("\nSupervisor finished!");
    exit(EXIT_SUCCESS);
}

// sets the default settings for shared mem object.
void default_shared_mem(struct shared_mem_options *shm)
{
    memset(shm, 0, sizeof(struct shared_mem_options));
    shm->prot = PROT_READ | PROT_WRITE;
    shm->perms = MAP_SHARED;
}

// creates a shared memory representing unnamed semaphores.
void create_semaphore(char *key)
{
    struct shared_mem_options shm;
    default_shared_mem(&shm);
    strcpy(shm.name, key);
    shm.flags = O_RDWR | O_CREAT;
    shm.size = sizeof(struct ipc_shared_mem);

    // create room.
    shm.fd = xshm_open(shm.name, shm.flags, S_IRWXU);
    xftruncate(shm.fd, shm.size);
    xmmap(NULL, shm.size, shm.prot, shm.perms, shm.fd, 0); // no ref, since we won't be using it in the parent process.
    xclose(shm.fd);
}

// creates a shared memory representing the room with given key.
void create_room(char *key, int size)
{
    // shared memory settings
    struct shared_mem_options shm;
    default_shared_mem(&shm);
    strcpy(shm.name, key);
    shm.flags = O_RDWR | O_CREAT; // creation flags.
    shm.size = size;

    // create room.
    shm.fd = xshm_open(shm.name, shm.flags, S_IRWXU);
    xftruncate(shm.fd, shm.size);
    xmmap(NULL, shm.size, shm.prot, shm.perms, shm.fd, 0); // no ref, since we won't be using it in the parent process.
    xclose(shm.fd);
}

// gets a shared memory representing the room with given key.
void *get_shared(char *key)
{
    // shared memory settings
    struct shared_mem_options shm;
    default_shared_mem(&shm);
    strcpy(shm.name, key);
    shm.flags = O_RDWR; // open flag.

    // get room.
    shm.fd = xshm_open(shm.name, shm.flags, 0);

    // get room size.
    struct stat sb;
    if (fstat(shm.fd, &sb) == -1)
        xerror(__func__, "fstat");
    shm.size = sb.st_size;

    void *shared = xmmap(NULL, shm.size, shm.prot, shm.perms, shm.fd, 0);
    xclose(shm.fd);
    return shared;
}

// removes the shared memory representing the kitchen.
void remove_shared(char *key)
{
    if (shm_unlink(key) == -1)
        xerror(__func__, "shm_unlink");
}

// sem wait.
void swait(sem_t *sem)
{
    if (sem_wait(sem) == -1)
        xerror(__func__, "sem_wait");
}

// sem post.
void spost(sem_t *sem)
{
    if (sem_post(sem) == -1)
        xerror(__func__, "sem_post");
}

void get_supply(char *supply)
{
    int total = args.L * args.M * FOOD_KIND;
    int fd = xopen(args.F, O_RDONLY);
    if (xread(fd, supply, total) != total)
        xerror(__func__, "read");
    close(fd);
}
#include <inttypes.h>

#define HIST_SIZE 10 

typedef struct node {
    unsigned int way_id; 
    struct node *prev;
    struct node *next;
    uint64_t tag;
} Node;

typedef struct set {
    Node *head;
    Node *tail;
    unsigned int setID;
    unsigned int missesPerSet;
    unsigned int missesPerAll;
    unsigned int nWays;

    unsigned int nAccessesPerAll;

    // to keep track of history of misses
    unsigned int missHistory[HIST_SIZE];
    unsigned int front;

    unsigned int accessHistory[HIST_SIZE];
} Set;
/* An array of queues to implement LRU */
/*
 * Array of nodes
 * |set|-->|queue head|<==>|queue node|<==>...<==>|queue tail|
 * |set|-->|queue head|<==>|queue node|<==>...<==>|queue tail|
 * |set|-->|queue head|<==>|queue node|<==>...<==>|queue tail|
 * |set|-->|queue head|<==>|queue node|<==>...<==>|queue tail|
 * |set|-->|queue head|<==>|queue node|<==>...<==>|queue tail|
 * |set|-->|queue head|<==>|queue node|<==>...<==>|queue tail|
 * |set|-->|queue head|<==>|queue node|<==>...<==>|queue tail|
 */
 
/* This is used to copy a Set to be manipulated */
typedef struct setc {
    unsigned int setID;
    unsigned int missesPerSet;
    unsigned int missesPerAll;
    unsigned int nWays;

    unsigned int nAccessesPerAll;
} Setc; 

/* move Node 'node' (representing a way) to end of queue at set*/
unsigned int enqueue(Set *set, Node *node);

/* Remove head of queue. Assumes that there is at least one node in queue */
Node *dequeue(Set *set);

/* remove node with tag 'tag'. If not found, return NULL. Assumes at least one node in queue */
Node *rmvqueue(Set *set, unsigned int tag);


/* put node on head of queue */
unsigned int enqueue_head(Set *set, Node *node);

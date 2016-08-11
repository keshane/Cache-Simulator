#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include <openssl/md5.h>
#include <zlib.h>
#include "lruqueue.h"
#include "RHcache.h"
#include "zcache.h"

#define ZWAYS 4
#define ZDEPTH 4

unsigned long long int hits = 0;
unsigned long long int misses = 0;
unsigned long long int loads = 0;
unsigned long long int stores = 0;
unsigned int limit = 0;
unsigned int minAccesses = 0;
unsigned int histSize = 7;


void die(char *s);
void simulate(mode cur_mode,
              unsigned int ways, 
              unsigned int sets, 
              unsigned int blsize, 
              unsigned int time, 
              gzFile *fp);

void zsimulate(unsigned int sets, gzFile *fp);

/* extract address from a line in fp */
int readAddr(gzFile *fp, char *addr);

/* access address with index set. The way is determined by enqueue/dequeue using set and hTable */
bool accessAddr(Set *set, uint64_t tag);

void printStats(Set *setTables[], unsigned int t, unsigned int len);

void reallocateBlocks(Set *setTables[], unsigned int len, unsigned int nways, unsigned int time);
bool moveBlock(Set *setTables[], unsigned int from, unsigned int to);
int cmpSet(const void *a, const void *b);
int fhash(uint64_t addr, unsigned char digest[]);

void zallocate(uint32_t hashes[], uint64_t addr, ZBlock cache[][ZWAYS], uint32_t time);
void copy_zblock(ZBlock *src, ZBlock *dst);
bool find_oldest(ZBlock *node, 
                 ZBlock cache[][ZWAYS], 
                 unsigned int depth, 
                 unsigned int *oldest_time, 
                 ZBlock *path[],
                 ZBlock *oldest_path[],
                 unsigned int *depth_at_oldest_path);
/*********************************************************************************/
void die(char *s) {
    fprintf(stderr, "%s", s);
    fprintf(stdout, "%s", "./RHcache mode w s b t input.txt\n");
    exit(1);
}
int cmpSet(const void *a, const void *b) {
    const Setc *setA = *(const Setc **)a;
    const Setc *setB = *(const Setc **)b;
    return (setA->missesPerAll - setB->missesPerAll);
//  float diff = setA->missRatio - setB->missRatio;
//  if (diff < 0)
//      return -1;
//  else if (diff > 0)
//      return 1;
//  else
//      return 0;
}

void reallocateBlocks(Set *setTables[], unsigned int len, unsigned int nways, unsigned int time) {
    Setc *setTableCopy[len];
    unsigned int missHistSum = 0;
    unsigned int accHistSum = 0;
    unsigned int missHistFront;

    for (int i = 0; i < len; i++) {
        setTableCopy[i] = malloc(sizeof(Set));
        setTableCopy[i]->setID = setTables[i]->setID;

        /* get miss history and add current miss count to history */
        missHistSum = setTables[i]->missesPerAll;
        accHistSum = setTables[i]->nAccessesPerAll;
        if (histSize > 0) {
            for (int j = 0; j < histSize; j++) {
                missHistSum += setTables[i]->missHistory[j];
                accHistSum += setTables[i]->accessHistory[j];
            }
            // faux queue implemented as array with a pointer to the head
            missHistFront = setTables[i]->front;
            setTables[i]->missHistory[missHistFront] = setTables[i]->missesPerAll;
            setTables[i]->accessHistory[missHistFront] = setTables[i]->nAccessesPerAll;
            setTables[i]->front++;
            setTables[i]->front = setTables[i]->front % histSize; 
        }

        setTableCopy[i]->missesPerAll = missHistSum;
        setTableCopy[i]->nAccessesPerAll = accHistSum;
        //avgMisses += setTableCopy[i]->missesPerAll;
        setTableCopy[i]->nWays = setTables[i]->nWays;
    }
    qsort(setTableCopy, len, sizeof(Set *), cmpSet);


    unsigned int setMax = len - 1;
    unsigned int setMin = 0;
    while(setTableCopy[setMax]->missesPerAll >= limit &&
          setTableCopy[setMin]->missesPerAll < limit) {
        // If able to move block, move on to next one. else try again with another set
        if (setTables[setMax]->nAccessesPerAll > minAccesses && 
            setTables[setMin]->nWays > nways / 2 &&
            moveBlock(setTables, setTableCopy[setMin]->setID, setTableCopy[setMax]->setID)) {
            setMax--;
            setMin++;
        }
        else if (setTables[setMax]->nAccessesPerAll <= minAccesses) {
            setMax--;
        }
        else {
            setMin++;
        }
    }
    
}

bool moveBlock(Set *setTables[], unsigned int from, unsigned int to) {
    Node *node;
    node = dequeue(setTables[from]);
    if (node == NULL) {
        return false;
    }
    else {
        node->tag = 0;
        enqueue_head(setTables[to], node);
        setTables[from]->nWays--;
        setTables[to]->nWays++;
        return true;
    }
}




bool accessAddr(Set *set, uint64_t tag) {
    Node *node;
    set->nAccessesPerAll++;

    if ((set->tail)->tag != tag) { // unnecessary to remove node from queue if it's already at the tail 
        node = rmvqueue(set, tag);

        if (node == NULL) { // miss
            node = dequeue(set);
            if (node == NULL) { // only one node in queue
                node = set->head;
                node->tag = tag;
            }
            else {
                node->tag = tag;
                enqueue(set, node);
            }
            set->missesPerAll++;
            set->missesPerSet++;
            misses++;
            return false;
        }
        else {
            enqueue(set, node);
            hits++;
        }
    }
    else 
        hits++; // found tag in tail. just need to increment hits

    return true;
}

void printStats(Set *setTable[], unsigned int t, unsigned int len) {
#ifdef OPT1
    printf("%d", t);
    for (int i = 0; i < len; i++)
        printf(", %d", setTable[i]->missesPerSet);
    printf("\n");
#endif
#ifdef OPT2
    printf("%d", t);
    for (int i = 0; i < len; i++)
        printf(", %d", setTable[i]->missesPerAll);
    printf("\n");
#endif
#if !defined(OPT1) && !defined(OPT2) && !defined(DEBUG)
    printf("%d\n", t);
    for (int i = 0; i < len; i++)
        printf("%d, %d, %d, %d\n", i+1, setTable[i]->nWays, setTable[i]->missesPerSet, setTable[i]->missesPerAll);
#endif
}


void simulate(mode cur_mode,
              unsigned int ways, 
              unsigned int sets, 
              unsigned int blsize, 
              unsigned int time, 
              gzFile *fp) {
    Set *setTable[sets];
    Set *set;

    char addrstr[64];
    uint64_t addr;
    unsigned int setstemp = sets;
    unsigned int blsizetemp = blsize;
    unsigned int setslog = 0;
    unsigned int blsizelog = 0;
    unsigned int tagLen;
    unsigned int load;
    uint64_t tag;
    uint64_t tagMask;
    uint64_t index;
    uint64_t indexMask;

    unsigned int tPerAll = 0; // keep track of time
    unsigned int tPerSet[sets];


    // Set up table for queues to implement LRU
    for (int set = 0; set < sets; set++) {
        setTable[set] = malloc(sizeof(Set)); // set up set
        setTable[set]->missesPerAll = 0;
        setTable[set]->missesPerSet = 0;
        setTable[set]->nWays = ways;
        setTable[set]->setID = set;
        setTable[set]->front = 0;
        setTable[set]->nAccessesPerAll = 0;
        for (int way = 0; way < ways; way++) {
            Node *prevNode, *curNode;
            curNode = malloc(sizeof(Node));
            curNode->way_id = way;

            if (ways == 1) { // if there is only one way
                setTable[set]->head = curNode;
                setTable[set]->tail = curNode;
                curNode->prev = NULL;
                curNode->next = NULL;
            }
            else if (way == 0) { // set first way as head
                setTable[set]->head = curNode;
                curNode->prev = NULL;
                curNode->next = NULL;
            }
            else if (way == ways - 1) { // last node
                prevNode->next = curNode;
                curNode->prev = prevNode;
                curNode->next = NULL;
                setTable[set]->tail = curNode;
            }
            else {
                prevNode->next = curNode;
                curNode->prev = prevNode;
            }

            prevNode = curNode;
        }
    }



    // Find len of each variable
    while (setstemp >>= 1) setslog++;
    while (blsizetemp >>= 1) blsizelog++;

    // Create tag mask
    tagLen = 64 - setslog - blsizelog;
    tagMask = (1ULL << tagLen) - 1;
    tagMask <<= (setslog + blsizelog);

    // Create index mask
    indexMask = (1ULL << setslog) - 1;
    indexMask <<= blsizelog;

#ifndef DEBUG
    // optimal history size depends on the time length
    if (time < 1000) {
        minAccesses = 1;
        histSize = 6;
    }
    else if (time < 10000) {
        minAccesses = 2;
        histSize = 3;
    }
    else {
        minAccesses = 2;
        histSize = 0;
    }
    limit = ways / 2;
#endif


    while ((load = readAddr(fp, addrstr))) {
        if (load == 1)
            loads++;
        else
            stores++;

        addr = strtol(addrstr, NULL, 0);

        // Get tag
        tag = addr & tagMask;
        tag >>= (setslog + blsizelog);

        // Get index
        index = addr & indexMask;
        index >>= blsizelog;

        set = setTable[index]; // access correct set

        tPerAll++;
        tPerSet[index]++;

        accessAddr(set, tag);

        if (tPerAll % time == 0) {
#ifndef DEBUG
//            printStats(setTable, tPerAll, sets);
#endif
            if (cur_mode == ROBINHOOD)
                reallocateBlocks(setTable, sets, ways, time);

            // Reset stats
            for (int i = 0; i < sets; i++) {
                setTable[i]->missesPerAll = 0;
                setTable[i]->nAccessesPerAll = 0;
            }
        }

        // Reset stats
        if (tPerSet[index] % time == 0)
            setTable[index]->missesPerSet = 0;

    }


#if !defined(OPT1) && !defined(OPT2)
    printf("hits: %llu, misses: %llu\n", hits, misses);
//  printf("loads: %llu, stores: %llu\n", loads, stores);
#endif


    for (int set = 0; set < sets; set++) {
        Node *curNode = setTable[set]->head;
        while (curNode != NULL) {
            Node *next = curNode->next;
            free(curNode);
            curNode = next;
        }
        free(setTable[set]);
    }
                
            

}

int readAddr(gzFile *fp, char *addr) {
    char buf[67];

    if (gzgets(*fp, buf, sizeof(buf)) != NULL) {
        strncpy(addr, buf + 3, strlen(buf) - 3 + 1);
        if (buf[0] == 'L')
            return 1;
        else
            return 2;
    }
    else
        return 0;
}

int fhash(uint64_t addr, unsigned char digest[]) {
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, &addr, sizeof(addr));
    MD5_Final(digest, &ctx);
    return 0;
}

void copy_zblock(ZBlock *src, ZBlock *dst) {
    for (int i = 0; i < ZWAYS; i++) {
        dst->hashes[i] = src->hashes[i];
    }
    dst->addr = src->addr;
    dst->time_stamp = src->time_stamp;
}

bool find_oldest(ZBlock *node, 
                 ZBlock cache[][ZWAYS], 
                 unsigned int depth, 
                 unsigned int *oldest_time, 
                 ZBlock *path[],
                 ZBlock *oldest_path[],
                 unsigned int *depth_at_oldest_path) {
    ZBlock *next;
    bool found_zero;


    path[depth] = node; // add to the path

    if (node->time_stamp < *oldest_time || (node->time_stamp == 0 && depth != 0) ) {
        *oldest_time = node->time_stamp;
        *depth_at_oldest_path = depth;
        /* Copy path to best path if oldest time */
        for (int i = 0; i <= depth; i++) {
            oldest_path[i] = path[i];
        }

        /* End search early if this block was unallocated */
        if (node->time_stamp == 0) {
            return true;
        }
    }


    if (depth + 1 <= ZDEPTH) {
        for (int i = 0; i < ZWAYS; i++) {
            next = &(cache[node->hashes[i]][i]);
            if (next == node) // one of these hashes points to the same block
                continue;
            found_zero = find_oldest(next, cache, depth + 1, oldest_time, path, oldest_path, depth_at_oldest_path);
            if (found_zero) {
                return true;
            }
        }
    }

    return false;
}




void zallocate(uint32_t hashes[], uint64_t addr, ZBlock cache[][ZWAYS], uint32_t time) {
    ZBlock root;
    ZBlock *path[ZDEPTH+1];
    ZBlock *oldest_path[ZDEPTH+1];
    unsigned int oldest_time;
    unsigned int depth_at_oldest_path;


    for (int i = 0; i < ZWAYS; i++) {
        root.hashes[i] = hashes[i];
        /* Checks for hits */
        if (cache[hashes[i]][i].addr == addr) {
            cache[hashes[i]][i].time_stamp = time;
            hits++;
            return;
        }
    }

    misses++;

    root.addr = addr;
    root.time_stamp = time;


    oldest_time = time;
    find_oldest(&root, cache, 0, &oldest_time, path, oldest_path, &depth_at_oldest_path);

    for (int i = depth_at_oldest_path; i > 0; i--) {
        copy_zblock(oldest_path[i-1], oldest_path[i]);
    }

    return;
}

void zsimulate(unsigned int sets, gzFile *fp) {
    int i, h;
    unsigned int access;
    char addrstr[64];
    uint64_t addr;
    uint32_t hashes[4];
    unsigned char digest[16];
    unsigned int set_power;
    uint32_t i0, i1, i2, i3;
    uint32_t mask;
    uint32_t time;

    ZBlock cache[sets][ZWAYS]; 

    for (int j = 0; j < sets; j++) {
        for (int k = 0; k < ZWAYS; k++) {
            cache[j][k].addr = 0;
            cache[j][k].time_stamp = 0;
        }
    }

    set_power = 0;
    while (sets >>= 1)
        set_power++;

    mask = (1 << set_power) - 1;

    time = 0;
    while((access = readAddr(fp, addrstr))) {
        if (access == 1)
            loads++;
        else
            stores++;
        addr = strtol(addrstr, NULL, 0);

        fhash(addr, digest);
        for (i = 0, h = 0; i < 16; i += 4, h++) {
            //fhash(addr + h, digest);
            i0 = digest[i]; // 8 bits --> 32 bits
            i1 = digest[i+1]; // 8 bits --> 32 bits
            i2 = digest[i+2]; // 8 bits --> 32 bits
            i3 = digest[i+3]; // 8 bits --> 32 bits
            hashes[h] = mask & (i0 << 24 | i1 << 16 | i2 << 8 | i3); // extract last set_power bits 
        }
        

        zallocate(hashes, addr, cache, time);

        time++;

    }
    printf("hits: %llu, misses: %llu\n", hits, misses);
//  printf("loads: %llu, stores: %llu\n", loads, stores);

} 

int main(int argc, char *argv[]) {
    mode cur_mode;
    int ways, sets, blsize, time;
    char *endptr;
    gzFile cfp;


    // Check command line arguments
    if (!(argc == 10  || argc == 7)) {
        die("Not enough arguments\n");
    }

    if (strcmp(argv[1], "standard") == 0)
        cur_mode = STANDARD;
    else if (strcmp(argv[1], "robinhood") == 0)
        cur_mode = ROBINHOOD;
    else if (strcmp(argv[1], "zcache") == 0)
        cur_mode = ZCACHE;
    else
        die("Not a valid mode\n");

    if ((ways = strtol(argv[2], &endptr, 10)) <= 0 || *endptr != '\0')
        die("ways is not a positive integer\n");
    if ((sets = strtol(argv[3], &endptr, 10)) <= 0 || *endptr != '\0')
        die("sets is not a positive integer\n");
    if ((blsize = strtol(argv[4], &endptr, 10)) <= 0 || *endptr != '\0')
        die("block size is not a positive integer\n");
    if ((time = strtol(argv[5], &endptr, 10)) <= 0 || *endptr != '\0')
        die("time interval is not a positive integer\n");
    if (access(argv[6], F_OK))
        die("Not a valid file\n");

#ifdef DEBUG
    if (argc > 7) {
        if ((limit = strtol(argv[7], &endptr, 10)) <= 0 || *endptr != '\0')
            die("limit is not positive integer\n");
        if ((minAccesses = strtol(argv[8], &endptr, 10)) <= 0 || *endptr != '\0')
            die("minAccesses is not a positive integer\n");
        if ((histSize = strtol(argv[9], &endptr, 10)) < 0 || *endptr != '\0')
            die("hist size is not a positive integer\n");
    }
#endif


    /* Check to see that the number of sets is a power of 2*/
    if (!((sets & (~sets + 1)) == sets))
        die("sets should be a power of 2\n");



    cfp = gzopen(argv[6], "r");
    if (cur_mode == ZCACHE) {
        if (ways != ZWAYS)
            die("ways must equal 4 for zcache\n");
        zsimulate(sets, &cfp);
    }
    else
        simulate(cur_mode, ways, sets, blsize, time, &cfp);

    gzclose(cfp);


    return 0;
}

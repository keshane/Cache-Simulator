#include <inttypes.h>


typedef struct znode {
    uint32_t hashes[4];
    uint64_t addr;
    unsigned int time_stamp;
} ZBlock;

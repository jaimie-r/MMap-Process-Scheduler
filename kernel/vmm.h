#ifndef _VMM_H_
#define _VMM_H_

#include "stdint.h"
#include "shared.h"
#include "physmem.h"

class Node;

namespace gheith {
    extern uint32_t* global_pd;
}

namespace VMM {

    // Called (on the initial core) to initialize data structures, etc
    extern void global_init();

    // Called on each core to do per-core initialization
    extern void per_core_init();

    // naive mmap
    extern void* naive_mmap(uint32_t size, Shared<Node> file, uint32_t file_offset);

    // naive munmap
    void naive_munmap(void* p);

}

#endif

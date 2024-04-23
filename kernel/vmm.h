#ifndef _VMM_H_
#define _VMM_H_

#include "stdint.h"
#include "ext2.h"

namespace gheith {
    extern uint32_t* make_pd();
    extern void delete_pd(uint32_t*);
    extern void delete_private(uint32_t*);
}

namespace VMM {

    // Called (on the initial core) to initialize data structures, etc
    extern void global_init();

    // Called on each core to do per-core initialization
    extern void per_core_init();

    extern void *mmap (void *addr, size_t length, int prot, int flags, int fd, off_t offset);

    extern int munmap (void *addr, size_t len);

}

struct NodeEntry {
        NodeEntry(Shared<Node> file, uint32_t pa) : file(file), pa(pa) {
            num_processes = 1;
            next = nullptr;
        }

        Shared<Node> file;
        uint32_t num_processes;
        uint32_t pa;
        NodeEntry* next;
    };

    struct VMEntry {
        VMEntry(Shared<Node> file, uint32_t size, uint32_t starting_address, uint32_t offset, VMEntry* next) :
                file(file), size(size), starting_address(starting_address), offset(offset), next(next) {};

        Shared<Node> file;
        uint32_t size;
        uint32_t starting_address;
        uint32_t offset;
        VMEntry* next;
        NodeEntry* node;
    };

#endif

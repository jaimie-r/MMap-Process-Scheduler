#ifndef _VMM_H_
#define _VMM_H_

#include "stdint.h"

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
}

#endif

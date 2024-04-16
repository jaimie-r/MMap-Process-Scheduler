#ifndef _physmem_h_
#define _physmem_h_

#include "stdint.h"

namespace PhysMem {
    constexpr uint32_t FRAME_SIZE = 1 << 12;

    void init(uint32_t start, uint32_t size);

    inline uint32_t offset(uint32_t pa) {
        return pa & 0xFFF;
    }

    inline uint32_t ppn(uint32_t pa) {
        return pa >> 12;
    }

    inline uint32_t framedown(uint32_t pa) {
        return (pa / FRAME_SIZE) * FRAME_SIZE;
    }

    inline uint32_t frameup(uint32_t pa) {
        return framedown(pa + FRAME_SIZE - 1);
    }

    uint32_t alloc_frame();

    void dealloc_frame(uint32_t);
}

#endif

#include "vmm.h"
#include "machine.h"
#include "idt.h"
#include "libk.h"
#include "blocking_lock.h"
#include "config.h"
#include "threads.h"
#include "debug.h"
#include "ext2.h"
#include "physmem.h"

namespace gheith {
    uint32_t* global_pd = nullptr;

    void map_pn(uint32_t* pd, uint32_t va, uint32_t pa) {
        uint32_t pdi = va >> 22;
        uint32_t pde = pd[pdi];
        if ((pde & 1) == 0) {
            pde = PhysMem::alloc_frame() | 7;
            pd[pdi] = pde;
        }
        uint32_t* pt = (uint32_t*) (pde & 0xFFFFF000);
        uint32_t pti = (va >> 12) & 0x3FF;
        pt[pti] = pa | 7;
    }

    void unmap_pn(uint32_t* pd, uint32_t va) {
        uint32_t pdi = va >> 22;
        uint32_t pde = pd[pdi];
        if ((pde & 1) == 0) return;
        uint32_t* pt = (uint32_t*) (pde & 0xFFFFF000);
        uint32_t pti = (va >> 12) & 0x3FF;
        if ((pt[pti] & 1) == 0) return;
        uint32_t page = (pt[pti] & 0xFFFFF000);
        PhysMem::dealloc_frame(page);
        pt[pti] = 0;
        invlpg(va);
    }
}

namespace VMM {

void global_init() {
    using namespace gheith;
    global_pd = (uint32_t*) PhysMem::alloc_frame();
    for (uint32_t va = 0x00001000; va < kConfig.memSize; va += PhysMem::FRAME_SIZE) {
        map_pn(global_pd, va, va);
    }
    map_pn(global_pd, kConfig.ioAPIC, kConfig.ioAPIC);
    map_pn(global_pd, kConfig.localAPIC, kConfig.localAPIC);
}

void per_core_init() {
    using namespace gheith;
    auto me = current();
    vmm_on((uint32_t)me->pd);
}

void naive_munmap(void* p_) {
    using namespace gheith;
    uint32_t va = PhysMem::framedown((uint32_t) p_);
    if (va < 0x80000000 || va == kConfig.ioAPIC || va == kConfig.localAPIC) return;
    auto me = current();
    VMEntry* prev = nullptr;
    VMEntry* temp = me->entry_list;
    while (temp != nullptr) {
        if (va >= temp->starting_address && va < temp->starting_address + temp->size) {
            if (prev == nullptr) {
                me->entry_list = temp->next;
            } else {
                prev->next = temp->next;
            }
            for (uint32_t va = temp->starting_address; va < temp->starting_address + temp->size; va += PhysMem::FRAME_SIZE) {
                unmap_pn(me->pd, va);
            }
            delete temp;
            return;
        }
        prev = temp;
        temp = temp->next;
    }
}

void* naive_mmap(uint32_t sz_, Shared<Node> node, uint32_t offset_) {
    using namespace gheith;
    auto me = current();
    
    uint32_t va = 0x80000000;
    uint32_t size = PhysMem::frameup(sz_);
    VMEntry* prev = nullptr;
    VMEntry* temp = me->entry_list;
    while (temp != nullptr) {
        if (va + size <= temp->starting_address) {
            break;
        }
        va = temp->starting_address + temp->size;
        prev = temp;
        temp = temp->next;
    }
    VMEntry* new_entry = new VMEntry(node, size, va, offset_, temp);
    if (prev == nullptr) {
        me->entry_list = new_entry;
    } else {
        prev->next = new_entry;
    }
    return (uint32_t*) va;
}

} /* namespace vmm */

extern "C" void vmm_pageFault(uintptr_t va_, uintptr_t *saveState) {
    using namespace gheith;
    auto me = current();
    auto va = PhysMem::framedown(va_);
    if (va >= 0x80000000) {
        auto temp = me->entry_list;
        while (temp != nullptr) {
            if (va >= temp->starting_address && va < temp->starting_address + temp->size) {
                auto pa = PhysMem::alloc_frame();
                map_pn(me->pd, va, pa);
                if (temp->file != nullptr) {
                    auto read = temp->file->read_all(temp->offset + va - temp->starting_address, PhysMem::FRAME_SIZE, (char*) pa);
                    if (read != PhysMem::FRAME_SIZE) {
                        if (read == -1) read = 0;
                        for (int i = 0; i < PhysMem::FRAME_SIZE - read; i++) {
                            ((char*) pa)[read + i] = 0;
                        }
                    }
                } else {
                    for (uint32_t i = 0; i < PhysMem::FRAME_SIZE; i++) {
                        ((char*) pa)[i] = 0;
                    }
                }
                return;
            }
            temp = temp->next;
        }
    }
    Debug::panic("*** can't handle page fault at %x\n",va_);
}

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
#include "process.h"
#include "priority_queue.h"


namespace gheith {

    using namespace PhysMem;

    uint32_t* shared = nullptr;
    NodeEntry* node_list = nullptr;

    void map(uint32_t* pd, uint32_t va, uint32_t pa) {
        auto pdi = va >> 22;
        auto pti = (va >> 12) & 0x3FF;
        auto pde = pd[pdi];
        if ((pde & 1) == 0) {
            pde = PhysMem::alloc_frame() | 7;
            pd[pdi] = pde;
        }
        auto pt = (uint32_t*) (pde & 0xFFFFF000);
        pt[pti] = pa | 3;
    }

    void user_map(uint32_t* pd, uint32_t va, uint32_t pa) {
        auto pdi = va >> 22;
        auto pti = (va >> 12) & 0x3FF;
        auto pde = pd[pdi];
        if ((pde & 1) == 0) {
            pde = PhysMem::alloc_frame() | 7;
            pd[pdi] = pde;
        }
        auto pt = (uint32_t*) (pde & 0xFFFFF000);
        pt[pti] = pa | 7;
    }

    void unmap(uint32_t* pd, uint32_t va, bool physical) {
        auto pdi = va >> 22;
        auto pti = (va >> 12) & 0x3FF;
        auto pde = pd[pdi];
        if ((pde & 1) == 0) return;
        auto pt = (uint32_t*) (pd[pdi] & 0xFFFFF000);
        auto pte = pt[pti];
        if ((pte & 1) == 0) return;
        auto pa = pte & 0xFFFFF000;
        pt[pti] = 0;
        if (physical) {
            dealloc_frame(pa);
        }
        invlpg(va);
    }

    bool is_special(uint32_t va) {
        return (va < 0x80000000) || (va == kConfig.ioAPIC) || (va == kConfig.localAPIC);
    }

    uint32_t* make_pd() {
        auto pd = (uint32_t*) PhysMem::alloc_frame();

        auto m4 = 4 * 1024 * 1024;
        auto shared_size = 4 * (((kConfig.memSize + m4 - 1) / m4));

        memcpy(pd,shared,shared_size);

        map(pd,kConfig.ioAPIC,kConfig.ioAPIC);
        map(pd,kConfig.localAPIC,kConfig.localAPIC);

        return pd;
    }

    void delete_private(uint32_t* pd) {
        ASSERT(uint32_t(pd) == getCR3());
        for (unsigned pdi=512; pdi<1024; pdi++) {
            auto pde = pd[pdi];
            if ((pde&1) == 0) continue;
            auto pt = (uint32_t*)(pde & 0xFFFFF000);

            bool contains_special = false;

            for (unsigned pti=0; pti<1024; pti++) {
                auto va = (pdi << 22) | (pti << 12);
                if (is_special(va)) {
                    contains_special = true;
                    continue;
                }
                auto pte = pt[pti];
                if ((pte&1) == 0) continue;
                pt[pti] = 0;
                auto frame = pte & 0xFFFFF000;
                dealloc_frame(frame);
                invlpg(va);
            }
            if (!contains_special) {
                pd[pdi] = 0;
                dealloc_frame((uint32_t)pt);
            }
        }
    }

    void delete_pd(uint32_t* pd) {
        ASSERT(uint32_t(pd) != getCR3());

        for (unsigned pdi=512; pdi<1024; pdi++) {
            auto pde = pd[pdi];
            if ((pde&1) == 0) continue;
            auto pt = (uint32_t*)(pde & 0xFFFFF000);

            for (unsigned pti=0; pti<1024; pti++) {
                auto va = (pdi << 22) | (pti << 12);

                auto pte = pt[pti];
                if ((pte&1) == 0) continue;
                auto frame = pte & 0xFFFFF000;
                if (!is_special(va)) {
                    dealloc_frame(frame);
                }
            }
            dealloc_frame((uint32_t)pt);
        }

        PhysMem::dealloc_frame((uint32_t)pd);
    }
}

namespace VMM {

void global_init() {
    using namespace gheith;
    shared = (uint32_t*) PhysMem::alloc_frame();

    for (uint32_t va = FRAME_SIZE; va < kConfig.memSize; va += FRAME_SIZE) {
        map(shared,va,va);
    }
}

void per_core_init() {
    using namespace gheith;

    Interrupts::protect([] {
        ASSERT(Interrupts::isDisabled());
        auto me = activeThreads[SMP::me()];
        vmm_on((uint32_t)me->process->pd);
    });
}

int munmap(void *addr, size_t len) {
    using namespace gheith;
    uint32_t address = (uint32_t) addr;
    if (is_special(address)) return -1;

    auto me = current();
    VMEntry* prev = nullptr;
    VMEntry* vm_entry = me->process->entry_list;

    // Iterate through entry list for entry associated with address.
    while (vm_entry != nullptr) {

        // Found the corresponding entry.
        if (address >= vm_entry->starting_address && address < vm_entry->starting_address + vm_entry->size) {
            bool physical = true;

            // Remove the entry from the process's entry list.
            if (prev == nullptr) {
                me->process->entry_list = vm_entry->next;
            } else {
                prev->next = vm_entry->next;
            }

            // If a private mapping, it needs to be deallocated from physical memory.
            if ((vm_entry->flags & 0x1) == 0) {
                physical = true;
            } else if (--vm_entry->node->num_processes != 0) {
                // Only deallocate from physical memory if this is the last process mapped to the file.
                physical = false;
            }

            // Unmap from virtual memory.
            for (uint32_t va = vm_entry->starting_address; va < vm_entry->starting_address + vm_entry->size; va += PhysMem::FRAME_SIZE) {
                unmap(me->process->pd, va, physical);
            }

            if (physical) {
                NodeEntry *node_entry = node_list;
                NodeEntry *next_entry = node_entry->next;
                if (node_entry->file->number == vm_entry->node->file->number) {
                    node_list = node_list->next;
                } else {
                    while (next_entry != nullptr) {
                        if (next_entry->file->number == vm_entry->node->file->number) {
                            node_entry->next = next_entry->next;
                            break;
                        }
                        node_entry = next_entry;
                        next_entry = next_entry->next;
                    }
                }
            }
            delete vm_entry;
            return 0;
        }
        prev = vm_entry;
        vm_entry = vm_entry->next;
    }
    return 0;
}

void *mmap (void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    using namespace gheith;

    // If address is not specified, default to first-fitting address.
    uint32_t va = addr == 0 ? 0x80000000 : (uint32_t) addr;
    if (is_special(va)) return nullptr;
    auto me = current();
    
    uint32_t size = PhysMem::frameup(length);
    VMEntry* prev = me->process->entry_list;
    VMEntry* temp = nullptr;

    // Iterate through entry list. Find best-fitting address.
    if (prev != nullptr) {
        temp = prev->next;
        while (temp != nullptr) {
            // Iterate through entries until past user-specified address.
            if (va >= temp->starting_address) {
                prev = temp;
                temp = temp->next;
                continue;
            }
            if (va >= prev->starting_address + prev->size && va + size < temp->starting_address) {
                break;
            }
            va = prev->starting_address + prev->size;
            if (va >= prev->starting_address + prev->size && va + size < temp->starting_address) {
                break;
            }
            prev = temp;
            temp = temp->next;
        }
    }
    // MAP_FIXED: Return nullptr if specified address is undesignated.
    if (va != (uint32_t) addr && (flags & 2) == 2) {
        return nullptr;
    }

    // Create file and add to entry list.
    Shared<Node> file = (Shared<Node>) nullptr;
    if (fd >= 0) {
        file = me->process->getFile(fd)->getNode();
    } 

    VMEntry* new_entry = new VMEntry(file, size, va, offset, temp, flags, prot);
    if (prev == nullptr) {
        me->process->entry_list = new_entry;
    } else {
        prev->next = new_entry;
    }
    return (uint32_t*) va;
}

}

extern "C" void vmm_pageFault(uintptr_t va_, uintptr_t *saveState) {
    using namespace gheith;
    auto me = current();
    ASSERT((uint32_t)me->process->pd == getCR3());
    ASSERT(me->saveArea.cr3 == getCR3());

    uint32_t va = PhysMem::framedown(va_);
    auto vm_entry = me->process->entry_list;

    // Iterate through entry list to find mapped file.
    if (va >= 0x80000000) {

        while (vm_entry != nullptr) {

            // Found the entry that the virtual address corresponds to.
            if (va >= vm_entry->starting_address && va < vm_entry->starting_address + vm_entry->size) {

                uint32_t pa;

                // If an anonymous or private mapping, allocate a new physical frame. If file has not
                // been mapped yet, allocate a new physical frame.
                if (vm_entry->file == nullptr || (vm_entry->flags & 0x1) == 0) {
                    pa = PhysMem::alloc_frame();
                } else {
                    // Process shares mapping with other processes. See if it has been mapped already.
                    NodeEntry* prev = nullptr;
                    NodeEntry* node_entry = node_list;
                    if (node_entry != nullptr) {
                        while (node_entry != nullptr) {
                            if (vm_entry->file->number == node_entry->file->number) {
                                break;
                            }
                            prev = node_entry;
                            node_entry = node_entry->next;
                        }
                    }
                    if (node_entry != nullptr) {
                        vm_entry->node = node_entry;
                        pa = node_entry->pa;
                        node_entry->num_processes++;
                    } else {
                        // File has not been mapped yet. We will read it in.
                        pa = PhysMem::alloc_frame();
                        NodeEntry* new_entry = new NodeEntry(vm_entry->file, pa);
                        if (prev == nullptr) {
                            node_list = new_entry;
                        } else {
                            prev->next = new_entry;
                        }
                        if (vm_entry->file != nullptr && (vm_entry->prot & 2) == 2) {
                            auto read = vm_entry->file->read_all(vm_entry->offset + va - vm_entry->starting_address, PhysMem::FRAME_SIZE, (char*) pa);
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
                        vm_entry->node = new_entry;
                    }
                }
                user_map(me->process->pd, va, pa);
                return;
            }
            vm_entry = vm_entry->next;
        }
    }
    current()->process->exit(1);
    stop();
}

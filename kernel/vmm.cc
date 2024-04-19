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


namespace gheith {

    using namespace PhysMem;

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

    void unmap(uint32_t* pd, uint32_t va) {
        auto pdi = va >> 22;
        auto pti = (va >> 12) & 0x3FF;
        auto pde = pd[pdi];
        if ((pde & 1) == 0) return;
        auto pt = (uint32_t*) (pd[pdi] & 0xFFFFF000);
        auto pte = pt[pti];
        if ((pte & 1) == 0) return;
        auto pa = pte & 0xFFFFF000;
        pt[pti] = 0;
        dealloc_frame(pa);
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

// void naive_munmap(void* p_) {
//     using namespace gheith;
//     uint32_t va = PhysMem::framedown((uint32_t) p_);
//     if (va < 0x80000000 || va == kConfig.ioAPIC || va == kConfig.localAPIC) return;
//     auto me = current();
//     VMEntry* prev = nullptr;
//     VMEntry* temp = me->process->entry_list;
//     while (temp != nullptr) {
//         if (va >= temp->starting_address && va < temp->starting_address + temp->size) {
//             if (prev == nullptr) {
//                 me->process->entry_list = temp->next;
//             } else {
//                 prev->next = temp->next;
//             }
//             for (uint32_t va = temp->starting_address; va < temp->starting_address + temp->size; va += PhysMem::FRAME_SIZE) {
//                 unmap(me->process->pd, va);
//             }
//             delete temp;
//             return;
//         }
//         prev = temp;
//         temp = temp->next;
//     }
// }

int munmap(void *addr, size_t len) {
    using namespace gheith;
    uint32_t address = (uint32_t)addr;
    if (address < 0x80000000 || address == kConfig.ioAPIC || address == kConfig.localAPIC) return -1;

    // round len up to page size divisible number
    uint32_t length = PhysMem::frameup(address);

    auto me = current();
    // remove from entry list
    VMEntry* prev = nullptr;
    VMEntry* temp = me->process->entry_list;
    while (temp != nullptr) {
        if (address >= temp->starting_address && address < temp->starting_address + temp->size) {
            if (prev == nullptr) {
                me->process->entry_list = temp->next;
            } else {
                prev->next = temp->next;
            }
            for (uint32_t va = temp->starting_address; va < temp->starting_address + length; va += PhysMem::FRAME_SIZE) {
                unmap(me->process->pd, va);
            }
            delete temp;
            return 0;
        }
        prev = temp;
        temp = temp->next;
    }
    // NodeEntry* nodeEntry = node_list;
    return 0;
}

// void* naive_mmap(uint32_t sz_, Shared<Node> node, uint32_t offset_) {
//     using namespace gheith;
//     auto me = current();
    
//     uint32_t va = 0x80000000;
//     uint32_t size = PhysMem::frameup(sz_);
//     VMEntry* prev = nullptr;
//     VMEntry* temp = me->process->entry_list;
//     while (temp != nullptr) {
//         if (va + size <= temp->starting_address) {
//             break;
//         }
//         va = temp->starting_address + temp->size;
//         prev = temp;
//         temp = temp->next;
//     }
//     VMEntry* new_entry = new VMEntry(node, size, va, offset_, temp);
//     if (prev == nullptr) {
//         me->process->entry_list = new_entry;
//     } else {
//         prev->next = new_entry;
//     }
//     return (uint32_t*) va;
// }


void *mmap (void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    using namespace gheith;
    auto me = current();
    uint32_t final_va = addr == 0 ? 0x80000000 : (uint32_t) addr;
    uint32_t va = 0x80000000;
    uint32_t size = PhysMem::frameup(length);
    VMEntry* prev = nullptr;
    VMEntry* temp = me->process->entry_list;
    while (temp != nullptr) {
        if (va + size <= temp->starting_address && va >= final_va) {
            break;
        }
        va = temp->starting_address + temp->size;
        prev = temp;
        temp = temp->next;
    }
    Shared<Node> file = (Shared<Node>)nullptr;
    if(fd >= 0) {
        file = me->process->getFile(fd)->getNode();
    } 
    VMEntry* new_entry = new VMEntry(file, size, va, offset, temp);
    if (prev == nullptr) {
        me->process->entry_list = new_entry;
    } else {
        prev->next = new_entry;
    }
    return (uint32_t*) va;
}

} /* namespace vmm */

extern "C" void vmm_pageFault(uintptr_t va_, uintptr_t *saveState) {
    using namespace gheith;
    Debug::printf("page fault at: %x\n", (int)va_);
    auto me = current();
    ASSERT((uint32_t)me->process->pd == getCR3());
    ASSERT(me->saveArea.cr3 == getCR3());

    uint32_t va = PhysMem::framedown(va_);
    auto temp = me->process->entry_list;

    // if (va >= 0x80000000 && temp == nullptr) {
    //     auto pa = PhysMem::alloc_frame();
    //     user_map(me->process->pd, va, pa);
    //     return;
    if (va >= 0x80000000) {
        // looping through entry list
        while (temp != nullptr) {
            Debug::printf("page fault 1\n");
            // finding correct vmentry
            if (va >= temp->starting_address && va < temp->starting_address + temp->size) {
                Debug::printf("page fault 2\n");
                uint32_t pa;
                // check if the vmentry is map anonymous (meaning file is null)
                if(temp->file == nullptr) {
                    // is map anonymous
                    // not in physmem yet so allocate
                    pa = PhysMem::alloc_frame();
                    for (uint32_t i = 0; i < PhysMem::FRAME_SIZE; i++) {
                        ((char*) pa)[i] = 0;
                    }
                } else {
                    // look for in node list
                    NodeEntry* prev = nullptr;
                    NodeEntry* temp2 = node_list;
                    if (temp2 != nullptr) {
                        while (temp2 != nullptr) {
                            // found entry in node list (meaning it exists in physmem alr)
                            if (temp->file->number == temp2->file->number) {
                                break;
                            }
                            Debug::printf("page fault 5\n");
                            prev = temp2;
                            temp2 = temp2->next;
                            Debug::printf("page fault 4\n");
                        }
                    }
                    if (temp2 != nullptr) {
                        // node is physmem
                        pa = temp2->pa;
                        temp2->num_processes++;
                    } else {
                        // not in physmem yet so allocate
                        pa = PhysMem::alloc_frame();
                        // add to node list
                        NodeEntry* new_entry = new NodeEntry(temp->file, pa);
                        if (prev == nullptr) {
                            node_list = new_entry;
                        } else {
                            prev->next = new_entry;
                        }
                        Debug::printf("page fault 4\n");
                        // reading in file
                        if (temp->file != nullptr) {
                            auto read = temp->file->read_all(temp->offset + va - temp->starting_address, PhysMem::FRAME_SIZE, (char*) pa);
                            // zero out the rest of the alloced space
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
                    }
                }
                user_map(me->process->pd, va, pa);
                Debug::printf("page fault done\n");
                return;
            }
            temp = temp->next;
        }
    }
    current()->process->exit(1);
    stop();
}

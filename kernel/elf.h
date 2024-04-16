#ifndef _ELF_H_
#define _ELF_H_

#include "stdint.h"
#include "ext2.h"

class ELF {
public:
    static uint32_t load(Shared<Node> file);
};

struct ElfHeader {
    unsigned char magic0; // should be 0x7f
    unsigned char magic1; // should be E
    unsigned char magic2; // should be L
    unsigned char magic3; // should be F
    uint8_t cls;   // 1 -> 32 bit, 2 -> 64 bit
    uint8_t encoding; // 1 -> LE, 2 -> BE 
    uint8_t header_version; // 1
    uint8_t abi; // 0 -> Unix System V, 1 -> HP-UX
    uint8_t abi_version; // 
    uint8_t padding[7];

    uint16_t type;            // 1 -> relocatable, 2 -> executable
    uint16_t machine;         // 3 -> intel i386
    uint32_t version;         // 1 -> current
    uint32_t entry;           // program entry point
    uint32_t phoff;           // offset in file for program headers
    uint32_t shoff;           // offset in file for section headers
    uint32_t flags;           // 
    uint16_t ehsize;          // how many bytes in this header
    uint16_t phentsize;       // bytes per program header entry
    uint16_t phnum;           // number of program header entries
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;

} __attribute__((packed));

struct ProgramHeader {
    uint32_t type;    /* 1 -> load, ... */
    uint32_t offset;  /* data offset in the file */
    uint32_t vaddr;   /* Where should it be loaded in virtual memory */
    uint32_t paddr;   /* ignore */
    uint32_t filesz;  /* how many bytes in the file */
    uint32_t memsz;   /* how many bytes in memory, result should be 0 */
    uint32_t flags;   /* 1 -> exec, 2 -> write, 4 -> read */
    uint32_t align;   /* alignment */
} __attribute__((packed));

#endif

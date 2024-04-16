#include "elf.h"
#include "machine.h"
#include "debug.h"

uint32_t ELF::load(Shared<Node> file) {
    ElfHeader hdr;

    file->read(0,hdr);

    uint32_t hoff = hdr.phoff;

    for (uint32_t i=0; i<hdr.phnum; i++) {
        ProgramHeader phdr;
        file->read(hoff,phdr);
        hoff += hdr.phentsize;

        if (phdr.type == 1) {
            char *p = (char*) phdr.vaddr;
            uint32_t memsz = phdr.memsz;
            uint32_t filesz = phdr.filesz;

            Debug::printf("vaddr:%x memsz:0x%x filesz:0x%x fileoff:%x\n",
                p,memsz,filesz,phdr.offset);
            file->read_all(phdr.offset,filesz,p);
            bzero(p + filesz, memsz - filesz);
        }
    }

    return hdr.entry;
}

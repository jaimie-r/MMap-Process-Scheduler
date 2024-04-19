#include "sys.h"
#include "stdint.h"
#include "idt.h"
#include "debug.h"
#include "threads.h"
#include "process.h"
#include "machine.h"
#include "ext2.h"
#include "elf.h"
#include "libk.h"
#include "file.h"
#include "heap.h"
#include "shared.h"
#include "kernel.h"

class FileDescriptor : public File {
    Shared<Node> node;
    uint32_t offset;

public:
    FileDescriptor(Shared<Node> node) : node(node), offset(0) {}
    bool isFile() override { return node->is_file(); }
    bool isDirectory() override { return node->is_dir(); }
    off_t seek(off_t offset) {
        this->offset = offset;
        return offset;
    }
    off_t size() { return node->size_in_bytes(); }
    ssize_t read(void* buffer, size_t n) {
        uint32_t read = node->read_all(offset, n, (char*) buffer);
        if (read > 0) {
            offset += read;
        }
        return read;
    }
    ssize_t write(void* buffer, size_t n) {
        return -1;
    }
    Shared<Node> getNode() {
        return node;
    }
};


int SYS::exec(const char* path,
              int argc,
              const char* argv[]
) {
    using namespace gheith;
    auto file = root_fs->find(root_fs->root,path); 
    if (file == nullptr) {
        return -1;
    }
    if (!file->is_file()) {
        return -1;
    }

    uint32_t sp = 0xefffe000;

    ElfHeader hdr;
    file->read(0,hdr);
    if (hdr.magic0 != 0x7F || hdr.magic1 != 'E' || hdr.magic2 != 'L' || hdr.magic3 != 'F') return -1;
    if (hdr.encoding != 1 || hdr.cls != 1) return -1;
    if (hdr.abi != 0 || hdr.version != 1 || hdr.type != 2 || hdr.phoff == 0) return -1;

    auto me = gheith::current();
    me->process->clear_private();

    uint32_t e = ELF::load(file);

    // mmap stack
    sp = (uint32_t)VMM::mmap((uint32_t *)sp, 4000000, 0,0, -1, 0);

    uint32_t total_bytes = 12 + (4 * argc);
    uint32_t* lengths = new uint32_t[argc];
    for (int i = 0; i < argc; i++) {
        lengths[i] = K::strlen(argv[i]) + 1;
        // Asked ChatGPT for way to align to 4-bytes.
        lengths[i] += (4 - (lengths[i] % 4)) % 4;
        total_bytes += lengths[i];
    }

    sp -= total_bytes;

    ((uint32_t*) sp)[0] = argc;

    char** argv_ptr = (char**) (sp + 8);
    ((uint32_t*) sp)[1] = (uint32_t) argv_ptr;

    char* argv_val = (char*) (sp + 8 + 4 * (argc + 1));
    
    for (int i = 0; i < argc; i++) {
        argv_ptr[i] = argv_val;
        memcpy(argv_val, argv[i], lengths[i]);
        argv_val[K::strlen(argv[i])] = 0;
        argv_val += lengths[i];
    }
    argv_ptr[argc] = 0;

    file = nullptr;

    switchToUser(e,sp,0);
    Debug::panic("*** implement switchToUser");
    return -1;
}

extern "C" int sysHandler(uint32_t eax, uint32_t *frame) {
    using namespace gheith;

    uint32_t *userEsp = (uint32_t*)frame[3];
    uint32_t userPC = frame[0];

    switch (eax) {
    case 0:
        {
            auto status = userEsp[1];
            current()->process->output->set(status);
            stop();
            return 0;
        }
    case 1: /* write */
        {
            int fd = (int) userEsp[1];
            char* buf = (char*) userEsp[2];
            if ((uint32_t) buf < 0x80000000 || (uint32_t) buf == kConfig.ioAPIC || (uint32_t) buf == kConfig.localAPIC) {
                return -1;
            }
            size_t nbyte = (size_t) userEsp[3];
            auto file = current()->process->getFile(fd);
            if (file == nullptr) return -1;
            return file->write(buf,nbyte);
        }
    case 2: /* fork */
    	{
		    int id = 0;
            auto child = current()->process->fork(id);
            thread(child, [userPC, userEsp]{
                switchToUser(userPC, (uint32_t) userEsp, 0);
            });
    		return id;
    	}
    case 3: /* sem */
        {
		    uint32_t initial = userEsp[1];
    		return current()->process->newSemaphore(initial);
        }

    case 4: /* up */
    	{
		    uint32_t id = userEsp[1];
            Shared<Semaphore> sem = current()->process->getSemaphore(id);
            if (sem == nullptr) {
                return -1;
            }
    		sem->up();
            return 0;
    	}
    case 5: /* down */
      	{
		    uint32_t id = userEsp[1];
            Shared<Semaphore> sem = current()->process->getSemaphore(id);
            if (sem == nullptr) {
                return -1;
            }
    		sem->down();
            return 0;
       	}
    case 6: /* close */
        {
            uint32_t id = userEsp[1];
            return current()->process->close(id);
        }
    case 7: /* shutdown */
		{
            Debug::shutdown();
            return -1;
        }
    case 8: /* wait */
        {
            uint32_t id = userEsp[1];
            uint32_t* status = (uint32_t*) userEsp[2];
            if ((uint32_t) status < 0x80000000 || (uint32_t) status == kConfig.ioAPIC || (uint32_t) status == kConfig.localAPIC) {
                return -1;
            }
            return current()->process->wait(id, status);
        }
    case 9: /* execl */
        {
            if ((char*) userEsp[1] == nullptr) return -1;
            uint32_t path_len = K::strlen((char*) userEsp[1]);
            char* path = new char[path_len + 1];
            memcpy(path, (char*) userEsp[1], path_len);
            path[path_len] = 0;
            uint32_t argc = 0;
            for (int i = 2; (char*) userEsp[i] != nullptr; i++) {
                argc++;
            }
            const char* argv[argc];
            for (uint32_t i = 0; i < argc; i++) {
                uint32_t arg_len = K::strlen((char*) userEsp[2 + i]);
                char* arg = new char[arg_len + 1];
                memcpy(arg, (char*) userEsp[2 + i], arg_len);
                arg[arg_len] = 0;
                argv[i] = arg;
            }
            return SYS::exec(path, argc, argv);
        }
    case 10: /* open */
        {
            char* filename = (char*) userEsp[1];
            if (filename == nullptr || (uint32_t) filename < 0x80000000) {
                return -1;
            }
            auto node = root_fs->find(root_fs->root, filename);
            if (node == nullptr) {
                return -1;
            }
            while (node->is_symlink()) {
                char symbol[node->size_in_bytes() + 1];
                node->get_symbol(symbol);
                symbol[node->size_in_bytes()] = 0;
                node = root_fs->find(root_fs->root, symbol);
            }
            Shared<File> file{new FileDescriptor(node)};
            return current()->process->setFile(file);
        }
    case 11: /* len */
        {
            uint32_t fd = userEsp[1];
            Shared<File> file = current()->process->getFile(fd);
            if (file == nullptr) {
                return -1;
            }
            return file->size();
        }
    case 12: /* read */
        {
            uint32_t fd = userEsp[1];
            if (fd == 1) {
                return -1;
            }
            char* buf = (char*) userEsp[2];
            if ((uint32_t) buf < 0x80000000 || (uint32_t) buf == kConfig.ioAPIC || (uint32_t) buf == kConfig.localAPIC) {
                return -1;
            }
            size_t nbyte = (size_t) userEsp[3];
            Shared<File> file = current()->process->getFile(fd);
            if (file == nullptr) {
                return -1;
            }
            return file->read(buf, nbyte);
        }
    case 13: /* seek */
        {
            uint32_t fd = userEsp[1];
            if (fd == 1) {
                return -1;
            }
            uint32_t offset = userEsp[2];
            Shared<File> file = current()->process->getFile(fd);
            if (file == nullptr) {
                return -1;
            }
            return file->seek(offset);
        }
    case 14: /* mmap */
        {
            //void *mmap (void *addr, size_t length, int prot, int flags, int fd, off_t offset);
            void *addr = (void *)userEsp[1];
            size_t length = (size_t)userEsp[2];
            int prot = (int)userEsp[3];
            int flags = (int)userEsp[4];
            int fd = (int)userEsp[5];
            off_t offset = (off_t)userEsp[6];
            void *va = VMM::mmap(addr, length, prot, flags, fd, offset);
            return (uint32_t)va; //idk if this is the correct way to do this.
        }
    case 15: /* munmap */
        {
            void *addr = (void *)userEsp[1];
            size_t length = (size_t)userEsp[2];
            return VMM::munmap(addr, length);
        }
    default:
        {
            Debug::printf("*** 1000000000 unknown system call %d\n",eax);
            return -1;
        }
    }
}   

void SYS::init(void) {
    IDT::trap(48,(uint32_t)sysHandler_,3);
}

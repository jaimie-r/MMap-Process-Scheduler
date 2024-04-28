#include "libc.h"

int main(int argc, char** argv) {
    printf("****************************\n");
    printf("*** MMAP AND MUNAP TESTS ***\n");
    printf("****************************\n");

    int fd = open("/etc/data.txt", 0);

    // Given a file descriptor number, mmap() will map the contents of the file to a default address
    // when 0 is passed in as the address.
    char* p = (char*) mmap(0, 1, 2, 1, fd, 0);
    printf("*** data.txt is now mapped to p.\n");
    printf("*** p is 0x%lx\n", (uint32_t) p);
    printf("*** printing p's contents:\n");
    printf("%s\n", p);
    printf("***\n");

    // Children processes inherit their parent's entry list.
    int child = fork();
    if (child == 0) {
        printf("*** we are in the child process.\n");
        printf("*** child processes inheit the parent's mappings. printing p's contents:\n");
        printf("%s\n", p);
        printf("***\n");
        exit(0);
    }
    uint32_t status = 42;
    wait(child, &status);

    // 1 flag indicates MAP_SHARED. Another mapping of this file will be shared across processes.
    printf("*** mapping the same file to p2. flag indicates shared mapping\n");
    char* p2 = (char*) mmap(0, 1, 2, 1, fd, 0);
    printf("*** p2 is 0x%lx\n", (uint32_t) p2);

    // Editing memory at address p2.
    p2[4] = 'x';

    // Even though the mapping associated with p2 was edited, the edit is seen with p's pointer.
    // These two different virtual addresses map to the same physical memory.
    printf("*** the region at p2 has been edited. now we will print p's contents. even though p and p2 are different\n***"
            " virtual addresses, the changes at address p2 are seen in p region.\n");
    printf("%s\n", p);
    printf("***\n");

    // Now we can test munmap().
    munmap(p2, 1);
    printf("*** p2 has been unmapped. printing p's contents:\n");
    printf("%s\n", p);
    printf("*** print successful. p2 did not deallocate physical memory since there is still another process.\n");

    munmap(p, 1);

    printf("*** all mappings of data.txt have been unmapped.\n");
    printf("***\n");
    
    // User can specify the address of their mapping.
    int fd2 = open("/etc/panic.txt", 0);
    char* p3 = (char*) mmap((void*) 0x90000000, 1, 2, 1, fd2, 0);
    printf("*** p3 is 0x%lx\n", (uint32_t) p3);

    char* p4 = (char*) mmap((void*) 0x90000000, 1, 2, 1, fd2, 0);
    printf("*** p4 is 0x%lx\n", (uint32_t) p4);

    // MAP_FIXED wants mapping at specified address or nullptr is returned.
    printf("*** fixed mapping ensures address is at specified location or nullptr is returned\n");
    char* p5 = (char*) mmap((void*) 0x90000000, 1, 2, 2, fd2, 0);
    printf("*** p5 is 0x%lx\n", (uint32_t) p5);

    // File contents should print.
    printf("%s\n", p3);

    // Process does not have read permissions. Attempts to read the region should fail.
    printf("*** mapping file without reading permission\n");
    char* p6 = (char*) mmap(0, 1, 0, 1, fd, 0);
    printf("*** printing p6 contents (should print nothing):\n");
    printf("%s\n", p6);

    shutdown();
    return 0;
}

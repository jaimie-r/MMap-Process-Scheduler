#include "libc.h"

void one(int fd) {
    printf("*** fd = %d\n",fd);
    printf("*** len = %d\n",len(fd));

    cp(fd,2);
}

int main(int argc, char** argv) {
    
    int fd = open("/etc/data.txt", 0);
    char* contents = (char*) mmap(0, 20, 0, 0, fd, 0);
    printf("*** 1\n");
    printf("%s\n", contents);
    printf("%lx\n", (uint32_t) contents);

    munmap(contents, 20);

    char* new_contents = (char*) mmap(0, 20, 0, 0, fd, 0);
    printf("%lx\n", (uint32_t) new_contents);

    shutdown();
    return 0;
}

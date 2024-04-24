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

    int child = fork();
    if(child == 0) {
        // child
        printf("child\n");
        munmap(contents, 20);
        contents = (char*) mmap(0, 20, 0, 0, fd, 0);
        printf("%s\n", contents);
        printf("%lx\n", (uint32_t) contents);
        contents[0] = 'X';
        exit(0);
    }
    uint32_t status = 42;
    wait(child, &status);
    printf("%c\n", contents[0]);

    printf("before unmap\n");
    munmap(contents, 20);

    char* new_contents = (char*) mmap(0, 20, 0, 0, fd, 0);
    printf("*** 2\n");
    printf("%s\n", new_contents);
    printf("%lx\n", (uint32_t) new_contents);

    int fd2 = open("/data/panic.txt", 0);
    printf("*** hello\n");
    char* c3 = (char*) mmap(0, 20, 0, 0, fd2, 0);
    printf("*** 3\n");
    printf("%s\n", c3);
    printf("%lx\n", (uint32_t) c3);

    shutdown();
    return 0;
}

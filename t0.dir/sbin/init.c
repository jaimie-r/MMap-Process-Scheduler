#include "libc.h"

void one(int fd) {
    printf("*** fd = %d\n",fd);
    printf("*** len = %d\n",len(fd));

    cp(fd,2);
}

int main(int argc, char** argv) {
    
    // int fd = open("/etc/data.txt",0);

    // char * contents = (char *)mmap(0, 20, 0, 0, fd, 0);
    // printf("*** 1\n");
    // printf("%s\n", contents);

    // munmap(contents, 20);
    // printf("%s\n", contents);

    int id = fork();
    if (id == 0) {
        printf("i'm the child\n");
    } else {
        //kill(id);
        wait(id, 0);
        printf("hi again\n");
        printf("i'm the parent\n");
    }

    shutdown();
    return 0;
}

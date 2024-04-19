#include "libc.h"

void one(int fd) {
    printf("*** fd = %d\n",fd);
    printf("*** len = %d\n",len(fd));

    cp(fd,2);
}

int main(int argc, char** argv) {
    printf("before open\n");
    int fd = open("/etc/data.txt",0);
    printf("after open\n");
    // one(fd);

    char * contents = (char *)mmap(0, 20, 0, 0, fd, 0);
    printf("*** 1\n");
    printf("%s", contents);

    shutdown();
    return 0;
}

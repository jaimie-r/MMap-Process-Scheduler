#ifndef _SYS_H_
#define _SYS_H_

class SYS {
public:
    static void init(void);
    static int exec(const char* path, int argc, const char* argv[]);
};

#endif

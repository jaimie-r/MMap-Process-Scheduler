#ifndef _KERNEL_H_
#define _KERNEL_H_

#include "shared.h"
#include "ext2.h"

void kernelMain(void);

namespace gheith {
    extern Shared<Ext2> root_fs;
}

#endif

#include "stdint.h"
#include "debug.h"
#include "ide.h"
#include "ext2.h"
#include "sys.h"
#include "threads.h"

const char* initName = "/sbin/init";


namespace gheith {
    Shared<Ext2> root_fs = Shared<Ext2>::make(Shared<Ide>::make(1));
}

void kernelMain(void) {
    auto argv = new const char* [2];
    argv[0] = "init";
    argv[1] = nullptr;
    
    int rc = SYS::exec(initName,1,argv);
    Debug::panic("*** rc = %d",rc);
}


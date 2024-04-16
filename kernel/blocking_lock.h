#ifndef _blocking_lock_h_
#define _blocking_lock_h_

#include "semaphore.h"
#include "atomic.h"
#include "shared.h"

class BlockingLock : public Semaphore {
public:
    inline BlockingLock() : Semaphore(1) {}

    inline void lock() { down(); }
    inline void unlock() { up(); }
    inline bool isMine() { return true; }
};



#endif


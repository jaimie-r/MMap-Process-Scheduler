#ifndef _barrier_h_
#define _barrier_h_

#include "atomic.h"
#include "semaphore.h"
#include "condition.h"
#include "debug.h"
#include "shared.h"

class Barrier {
    Atomic<int32_t> count;
    Semaphore sem;
    Atomic<uint32_t> ref_count;
public:

    Barrier(uint32_t count) : count(count),sem(0), ref_count(0) {}

    Barrier(const Barrier&) = delete;
    
    void sync() {
        int x = count.add_fetch(-1);
        if (x < 0) Debug::panic("count went negative in barrier");
        if (x == 0) {
            sem.up();
        } else {
            sem.down();
            sem.up();
        }
    }

    friend class Shared<Barrier>;
};

class ReusableBarrier {
    Shared<InterruptSafeLock> the_lock;
    Condition newEpoch;
    uint32_t const initial_count;
    volatile uint32_t count;
    volatile uint64_t epoch;
    Atomic<uint32_t> ref_count;
public:

    ReusableBarrier(uint32_t count): the_lock(new InterruptSafeLock()), newEpoch(the_lock), initial_count(count), count(count), epoch(0), ref_count(0) {
    }

    ReusableBarrier(const ReusableBarrier&) = delete;

    void sync() {
        the_lock->lock();
        
        ASSERT(count != 0);

        auto t = epoch;
        count --;

        if (count == 0) {
            epoch ++;
            count = initial_count;
            newEpoch.notifyAll();
        } else {
            while (epoch <= t) {
                newEpoch.wait();
            }
        }

        the_lock->unlock();
    }

    friend class Shared<ReusableBarrier>;

};
        
        
        

#endif


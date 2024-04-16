#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include "stdint.h"
#include "atomic.h"
#include "queue.h"
#include "threads.h"

class Semaphore {
    uint64_t volatile count;
    ISL lock;
    Queue<gheith::TCB,NoLock> waiting;
public:
    Atomic<uint32_t> ref_count;
    Semaphore(const uint32_t count) : count(count), lock(), waiting(), ref_count(0) {}

    Semaphore(const Semaphore&) = delete;

    void down() {
        using namespace gheith;

        auto was = lock.lock();
            
        if (count > 0) {
            count--;
            lock.unlock(was);
            return;
        }

        // We have to block because we don't check again
        block(BlockOption::MustBlock,[this](TCB* me) {

            ASSERT(!me->isIdle);

            waiting.add(me);
            lock.unlock(true); // It is safe to release the lock but the
                               // block contract requires interrupts to stay disabled
        });

        // we're back, we managed to subtract 1 and interrupts are disabled
        if (was) cli(); else sti();
    }

    void up() {
        using namespace gheith;

        auto was = lock.lock();
        auto next = waiting.remove();
        if (next == nullptr) {
            count ++;
        }
        lock.unlock(was);
        
        if (next != nullptr) {
            schedule(next);    
        }
    }
    
};

#endif


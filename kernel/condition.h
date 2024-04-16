#ifndef _kernel_condition_h_
#define _kernel_condition_h_

#include "queue.h"
#include "atomic.h"
#include "threads.h"
#include "atomic.h"
#include "shared.h"
#include "blocking_lock.h"

// A Mesa-style condition variable
//
//    - wait() can have false postivies: can return even though the condition hasn't been notified
//             and doesn't make owner guarantees
//

class Condition {

    Shared<InterruptSafeLock> the_lock;
    uint64_t epoch;
    Queue<gheith::TCB,NoLock> queue;
    Atomic<uint32_t> ref_count;
public:

    Condition(Shared<InterruptSafeLock> the_lock) : the_lock(the_lock), epoch(), queue(), ref_count(0) {}

    Condition(const Condition&) = delete;
    Condition(Condition&&) = delete;
    Condition& operator==(const Condition&) = delete;
    Condition& operator==(Condition&&) = delete;

    // Pre-condition: holding the lock
    // Post-condition: holding the lock
    // Guarantee: no missed signals
    // Non guarantee: condition is met. The caller should re-check.
    //
    // Why do we allow false positives (return without being signalled and/or allow
    // the state to change after you've been signalled)?
    //
    //     -   We implement Mesa-style monitors in which the lock is not
    //         transfered from the signaller to the waiting thread. This
    //         means that when a thread wakes up in wait, it has to reacquire
    //         the lock. This gives a chance to other threads to enter the
    //         monitor and mess with the state.
    //
    //         We can implement lock transfer and tighten that condition but:
    //
    //             - we decided below not to hold a lock across the context switch
    //
    //             - lock transfer can lead to subtle programming errors when
    //               the caller of signal forgets that the lock is lost
    //
    //         This also makes us immune against software that generates false notifies.
    //

    void wait() {
        using namespace gheith;

        // check the pre-condition
        ASSERT(the_lock->isMine());

        // We're holding the lock so interrupts must be disabled
        ASSERT(Interrupts::isDisabled());

        // remember the epoch
        auto old_epoch = epoch;

        // We will release the lock and use the epoch as out signal
        the_lock->unlock();

        // We can handle false positives, let's not insist on blocking
        block(BlockOption::MustBlock,[this,old_epoch](TCB* me) {

            // get the lock again
            the_lock->lock();

            if (epoch == old_epoch) {
                queue.add(me);
                the_lock->unlock();
            } else {
                ASSERT(epoch > old_epoch);
                the_lock->unlock();
                schedule(me);
            }
        });

        ASSERT(!Interrupts::isDisabled());

        the_lock->lock();

        // The post-condition is trivially satisfied but might as well be explicit about it
        ASSERT(the_lock->isMine());
    }

    // Pre-condition: has the lock
    // Post-condition: has the lock
    // Guarantee: will never release the lock
    void notify(const uint64_t limit) {
        using namespace gheith;

        ASSERT(the_lock->isMine());

        epoch += 1;

        for (uint64_t i=0; i<limit; i++) {
            auto next = queue.remove();
            if (next == nullptr) break;
            schedule(next);
        }

        ASSERT(the_lock->isMine());
    }

    // Pre-condition: has the lock
    // Post-condition: has the lock
    // Guarantee: will never release the lock   
    inline void notifyOne() {
        notify(1);
    }

    // Pre-condition: has the lock
    // Post-condition: has the lock
    // Guarantee: will never release the lock   
    inline void notifyAll() {
        notify(~((uint64_t)0));
    }

    friend class Shared<Condition>;

};


#endif

#ifndef _threads_h_
#define _threads_h_

#include "atomic.h"
#include "queue.h"
#include "heap.h"
#include "debug.h"
#include "smp.h"
#include "shared.h"
#include "vmm.h"
#include "tss.h"

class Process;

namespace gheith {

    constexpr static int STACK_BYTES = 8 * 1024;
    constexpr static int STACK_WORDS = STACK_BYTES / sizeof(uint32_t);

    struct TCB;

    struct SaveArea {
        uint32_t ebx;                        /* 0 */
        uint32_t esp;                        /* 4 */
        uint32_t ebp;                        /* 8 */
        uint32_t esi;                        /* 12 */
        uint32_t edi;                        /* 16 */
        volatile uint32_t no_preempt;        /* 20 */
        TCB* tcb;                            /* 24 */
        uint32_t cr2;                        /* 28 */
        uint32_t cr3;                        /* 32 */
    };

    struct TCB {
        static Atomic<uint32_t> next_id;

        const bool isIdle;
        const uint32_t id;

        // queue stuff
        TCB* next;

        SaveArea saveArea;

        Shared<Process> process;

        TCB(Shared<Process> process, bool isIdle);

        virtual ~TCB();

        virtual void doYourThing() = 0;
        virtual uint32_t interruptEsp() = 0;
    };

    extern "C" void gheith_contextSwitch(gheith::SaveArea *, gheith::SaveArea *, void* action, void* arg);

    extern TCB** activeThreads;
    extern TCB** idleThreads;

    extern TCB* current();
    extern Queue<TCB,InterruptSafeLock> readyQ;
    extern void entry();
    extern void schedule(TCB*);
    extern void delete_zombies();

    template <typename F>
    void caller(SaveArea* sa, F* f) {
        (*f)(sa->tcb);
    }
    
    // There is a bit of template/lambda voodoo here that allows us to
    // call block like this:
    //
    //   block([](TCB* previousTCB) {
    //        // do any cleanup work here but remember that you are:
    //        //     - running on the target stack
    //        //     - interrupts are disabled so you better be quick
    //   }

    enum class BlockOption {
        MustBlock,
        CanReturn
    };

    template <typename F>
    void block(BlockOption blockOption, const F& f) {

        uint32_t core_id;
        TCB* me;

        Interrupts::protect([&core_id,&me] {
            core_id = SMP::me();
            me = activeThreads[core_id];
            me->saveArea.no_preempt = 1;
        });
        
    again:
        readyQ.monitor_add();
        auto next_tcb = readyQ.remove();
        if (next_tcb == nullptr) {
            if (blockOption == BlockOption::CanReturn) return;
            if (me->isIdle) {
                // Many students had problems with hopping idle threads
                ASSERT(core_id == SMP::me());
                ASSERT(!Interrupts::isDisabled());
                ASSERT(me == idleThreads[core_id]);
                ASSERT(me == activeThreads[core_id]);
                iAmStuckInALoop(true);
                goto again;
            }
            next_tcb = idleThreads[core_id];    
        }

        next_tcb->saveArea.no_preempt = 1;

        activeThreads[core_id] = next_tcb;  // Why is this safe?

        tss[core_id].esp0 = next_tcb->interruptEsp();
        gheith_contextSwitch(&me->saveArea,&next_tcb->saveArea,(void *)caller<F>,(void*)&f);
    }

    struct TCBWithStack : public TCB {
        uint32_t *stack = new uint32_t[STACK_WORDS];
    
        TCBWithStack(Shared<Process> process);

        ~TCBWithStack();

        uint32_t interruptEsp() override {
            return (uint32_t) &stack[2047];
        }
    };
    

    template <typename T>
    struct TCBImpl : public TCBWithStack {
        T work;

        TCBImpl(Shared<Process> process, T work) : TCBWithStack(process), work(work) {
        }

        ~TCBImpl() {
        }

        void doYourThing() override {
            work();
        }
    };

    
};

extern void threadsInit();

extern void stop();
extern void yield();


template <typename T>
void thread(Shared<Process> process, T work) {
    using namespace gheith;

    delete_zombies();

    auto tcb = new TCBImpl<T>(process,work);
    schedule(tcb);

}



#endif

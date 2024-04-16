#ifndef _future_h_
#define _future_h_

#include "atomic.h"
#include "semaphore.h"
#include "threads.h"
#include "shared.h"


template <typename T>
class Future {
    Semaphore go;
    bool volatile isReady;
    T volatile    t;

    // Needed by Shared<>
    Atomic<uint32_t> ref_count;
public:

    Future() : go(0), isReady(false), t(), ref_count(0) {}

    // Can't copy a future
    Future(const Future&) = delete;
    Future& operator=(const Future& rhs) = delete;
    Future& operator=(Future&& rhs) = delete;
    
    void set(T v) {
        ASSERT(!isReady);
        t = v;
        isReady = true;
        go.up();
    }
    T get() {
        if (!isReady) {
            go.down();
            go.up();
        }
        return t;
    }

    friend class Shared<Future<T>>;
};

namespace gheith {
    template <typename Out, typename T>
    struct TaskImpl : public TCBWithStack {
        T work;
        Shared<Future<Out>> f;
    
        TaskImpl(T work, Shared<Future<Out>> f) : TCBWithStack(), work(work), f(f) {
        }

        ~TaskImpl() {
        }

        void doYourThing() override {
            Out out = work();
            f->set(out);
        }
    };
}

template <typename Out, typename T>
Shared<Future<Out>> future(T work) {
    using namespace gheith;

    delete_zombies();

    auto f = Shared<Future<Out>>::make();

    auto tcb = new TaskImpl<Out,T>(work,f);
    schedule(tcb);
    return f;
}



#endif


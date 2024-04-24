#ifndef _priority_queue_h_
#define _priority_queue_h_

#include "atomic.h"

template <typename T, typename LockType>
class PriorityQueue {
    T * volatile first = nullptr;
    uint32_t size = 0;
    LockType lock;

public:
    PriorityQueue() : first(nullptr), lock() {}
    PriorityQueue(const PriorityQueue&) = delete;

    void monitor_add() {
        monitor((uintptr_t)&size);
    }

    void monitor_remove() {
        monitor((uintptr_t)&first);
    }

    void add(T* t) {
        LockGuard g{lock};

        if (first == nullptr || t->process->run_time->get() < first->process->run_time->get()) {
        t->next = first;
        first = t;
        size++;
        return;
    }

    T* prev = nullptr;
    T* temp = first;
    while (temp != nullptr && temp->process->run_time->get() <= t->process->run_time->get()) {
        prev = temp;
        temp = temp->next;
    }

    prev->next = t;
    t->next = temp;
    }

    T* remove() {
        LockGuard g{lock};
        if (first == nullptr) {
            return nullptr;
        }
        auto it = first;
        first = it->next;
        return it;
    }

    T* remove_all() {
        LockGuard g{lock};
        auto it = first;
        first = nullptr;
        return it;
    }
};

#endif

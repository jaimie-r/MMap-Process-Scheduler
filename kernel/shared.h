// #ifndef _shared_h_
// #define _shared_h_

// #include "debug.h"

// template <typename T>
// class Shared {
//     T* ptr;

//     void drop() {
//         if (ptr != nullptr) {
//             auto new_count = ptr->ref_count.add_fetch(-1);
//             if (new_count == 0) {
//                 delete ptr;
//                 ptr = nullptr;
//             }
//         }
//     }

//     void add() {
//         if (ptr != nullptr) {
//             ptr->ref_count.add_fetch(1);
//         }
//     }

// public:

//     explicit Shared(T* it) : ptr(it) {
//         add();
//     }

//     //
//     // Shared<Thing> a{};
//     //
//     Shared(): ptr(nullptr) {

//     }

//     //
//     // Shared<Thing> b { a };
//     // Shared<Thing> c = b;
//     // f(b);
//     // return c;
//     //
//     Shared(const Shared& rhs): ptr(rhs.ptr) {
//         add();
//     }

//     //
//     // Shared<Thing> d = g();
//     //
//     Shared(Shared&& rhs): ptr(rhs.ptr) {
//         rhs.ptr = nullptr;
//     }

//     ~Shared() {
//         drop();
//     }

//     // d->m();
//     T* operator -> () const {
//         return ptr;
//     }

//     // d = nullptr;
//     // d = new Thing{};
//     Shared<T>& operator=(T* rhs) {
//         if (this->ptr != rhs) {
//             drop();
//             this->ptr = rhs;
//             add();
//         }
//         return *this;
//     }

//     // d = a;
//     // d = Thing{};
//     Shared<T>& operator=(const Shared<T>& rhs) {
//         auto other_ptr = rhs.ptr;
//         if (ptr != other_ptr) {
//             drop();
//             ptr = other_ptr;
//             add();
//         }
//         return *this;
//     }

//     // d = g();
//     Shared<T>& operator=(Shared<T>&& rhs) {
//         drop();
//         ptr = rhs.ptr;
//         rhs.ptr = nullptr;
        
//         return *this;
//     }

//     bool operator==(const Shared<T>& rhs) const {
// 	    return ptr == rhs.ptr;
//     }

//     bool operator!=(const Shared<T>& rhs) const {
// 	    return ptr != rhs.ptr;
//     }

//     bool operator==(T* rhs) {
//         return ptr == rhs;
//     }

//     bool operator!=(T* rhs) {
//         return ptr != rhs;
//     }

//     // e = Shared<Thing>::make(1,2,3);
//     template <typename... Args>
//     static Shared<T> make(Args... args) {
//         return Shared<T>{new T(args...)};
//     }

// };

// #endif

#ifndef _shared_h_
#define _shared_h_

#include "debug.h"

template <typename T>
class Shared {
    struct FatPointer {
        T* pointer;
        Atomic<int>* counter;
    };

    FatPointer* fatPointer;

    void decrementCounter() {
        if (fatPointer != nullptr && fatPointer->pointer != nullptr && fatPointer->counter->add_fetch(-1) == 0) {
            delete fatPointer->pointer;
            delete fatPointer->counter;
            delete fatPointer;
        }
    }

    void incrementCounter() {
        if (fatPointer->pointer != nullptr) {
            fatPointer->counter->add_fetch(1);
        }
    }

public:
    // single argument constructor
    explicit Shared(T* it) : fatPointer(new FatPointer{it, new Atomic<int>(0)}) {
        incrementCounter();
    }

    // default constructor
    Shared() : fatPointer(new FatPointer{nullptr, new Atomic<int>(0)}) {}

    // copy constructor
    Shared(Shared& rhs) : fatPointer(rhs.fatPointer) {
        incrementCounter();
    }

    // move constructor
    Shared(Shared&& rhs) : fatPointer(rhs.fatPointer) {
        rhs.fatPointer = nullptr;
    }

    // destructor
    ~Shared() {
        decrementCounter();
    }

    // dereference overload
    T* operator -> () const {
        return fatPointer->pointer;
    }

    // assignment operator
    Shared<T>& operator = (T* rhs) {
        if (fatPointer->pointer != rhs) {
            decrementCounter();
            fatPointer = new FatPointer{rhs, new Atomic<int>(0)};
            incrementCounter();
        }
        return *this;
    }

    // assignment operator
    Shared<T>& operator = (Shared<T>& rhs) {
        if (fatPointer != rhs.fatPointer) {
            decrementCounter();
            fatPointer = rhs.fatPointer;
            incrementCounter();
        }
        return *this;
    }

    // assignment operator
    Shared<T>& operator = (Shared<T>&& rhs) {
        decrementCounter();
        fatPointer = rhs.fatPointer;
        rhs.fatPointer = nullptr;
        return *this;
    }

    // equals operator with shared pointer
    bool operator == (const Shared<T>& rhs) const {
        if (fatPointer == nullptr || fatPointer->pointer == nullptr) {
            return rhs.fatPointer == nullptr || rhs.fatPointer->pointer == nullptr;
        }
        return fatPointer == rhs.fatPointer;
    }

    // not equals operator with shared pointer
    bool operator != (const Shared<T>& rhs) const {
        if (fatPointer == nullptr || fatPointer->pointer == nullptr) {
            return rhs.fatPointer != nullptr && rhs.fatPointer->pointer != nullptr;
        }
        return fatPointer != rhs.fatPointer;
    }

    // equals operator with real pointer
    bool operator == (T* rhs) {
        if (fatPointer == nullptr || fatPointer->pointer == nullptr) {
            return rhs == nullptr;
        }
        return fatPointer->pointer == rhs;
    }

    // not equals operator with real pointer
    bool operator != (T* rhs) {
        if (fatPointer == nullptr || fatPointer->pointer == nullptr) {
            return rhs != nullptr;
        }
        return fatPointer->pointer != rhs;
    }

    template <typename... Args>
    static Shared<T> make(Args... args) {
        return Shared<T>{new T(args...)};
    }
};

#endif

#ifndef _LOOP_H_
#define _LOOP_H_

#include "machine.h"

// Called when the code is spinning in a loop
//
// use_mwait (if true) tells us that the caller recommends using
// mwait. They are responsible for calling monitor
// as needed before checking the condition and calling us.
//
inline void iAmStuckInALoop(bool use_mwait) {
    if (onHypervisor) {
        // I'm going to assume it is QEMU
        // QEMU's support for pause and mwait is lousy and inconsistent
        // This gives me a place to experiment.
        if (use_mwait) {
            mwait();
        } else {
            asm volatile("pause");
        }
    } else {
        if (use_mwait) {
            mwait();
        } else {
            asm volatile("pause");
        }
    }
}



#endif

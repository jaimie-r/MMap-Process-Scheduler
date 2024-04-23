#ifndef _PROCESS_H_
#define _PROCESS_H_

#include "atomic.h"
#include "stdint.h"
#include "shared.h"
#include "vmm.h"
#include "blocking_lock.h"
#include "future.h"
#include "file.h"
#include "u8250.h"
#include "shared.h"
#include "ext2.h"

class Process {
	constexpr static int NSEM = 10;
	constexpr static int NCHILD = 10;
    constexpr static int NFILE = 10;

    Shared<File> files[NFILE]{};
	Shared<Semaphore> sems[NSEM]{};
	Shared<Future<uint32_t>> children[NCHILD]{};
    Shared<Atomic<bool>> kill_flags[NCHILD]{};
	BlockingLock mutex{};

	int getChildIndex(int id);
	int getSemaphoreIndex(int id);
	int getFileIndex(int id);

    Atomic<uint32_t> ref_count {0};

public:
    Shared<Future<uint32_t>> output = Shared<Future<uint32_t>>::make();// { new Future<uint32_t>() };
    Shared<Atomic<bool>> kill_flag = Shared<Atomic<bool>>::make(false);
    uint32_t *pd = gheith::make_pd();
    VMEntry* entry_list = nullptr;

    static Shared<Process> kernelProcess;

	Process(bool isInit);
	virtual ~Process();

	Shared<Process> fork(int& id);
    void clear_private();

	int newSemaphore(uint32_t init);

	Shared<Semaphore> getSemaphore(int id);

    Shared<File> getFile(int fd) {
        auto i = getFileIndex(fd);
        if (i < 0) {
            return Shared<File>();
        }
        return files[fd];
    }

    int setFile(Shared<File> file) {
        for (auto i = 0; i<NFILE; i++) {
            auto f = files[i];
            if (f == nullptr) {
                files[i] = file;
                return i;
            }
        }
        return -1;
    }

    
	int close(int id);
	void exit(uint32_t v) {
		output->set(v);
	}
	int wait(int id, uint32_t* ptr);

    int kill(int id);

	static void init(void);

    friend class Shared<Process>;
    //friend class gheith::TCB;
};

#endif

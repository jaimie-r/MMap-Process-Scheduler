#include "process.h"
#include "physmem.h"
#include "debug.h"
#include "vmm.h"

// id encoding
//   upper bit -> sign
//   3 bits -> kind
//   28 bits -> index

constexpr static uint32_t FL = 0x00000000;
constexpr static uint32_t PROC = 0x10000000;
constexpr static uint32_t SEM = 0x20000000;
constexpr static uint32_t INDEX_MASK = 0x0FFFFFFF;

Shared<Process> Process::kernelProcess = Shared<Process>::make(true);


void Process::init(void) {
}

Process::Process(bool isInit) {
	if (isInit) {
		Shared<File> f{ new U8250File(new U8250()) };
        files[0] = f;
        files[1] = f;
        files[2] = f;
    }
}

Process::~Process() {
	gheith::delete_pd(pd);

}

int Process::newSemaphore(uint32_t init) {
	LockGuard<BlockingLock> lock { mutex };

	for (int i=0; i<NSEM; i++) {
		auto p = sems[i];
		if (p == nullptr) {
			sems[i] = Shared<Semaphore>::make(init);
			return SEM | (i & INDEX_MASK);
		}
	}
	return -1;
}

Shared<Semaphore> Process::getSemaphore(int id) {
	LockGuard<BlockingLock> g { mutex };

	int idx = id & INDEX_MASK;
	if ((idx >= NSEM) || ((id & 0xF0000000) != SEM)) {
		return Shared<Semaphore>{};
	}
	return sems[idx];
}

void Process::clear_private() {
	using namespace gheith;

	LockGuard<BlockingLock> g { mutex };

	delete_private(pd);
}

Shared<Process> Process::fork(int& id) {
	LockGuard<BlockingLock> lock { mutex };

	int index = -1;

	for (auto i = 0; i<NCHILD; i++) {
		auto e = children[i];
		if (e == nullptr) {
			index = i;
			break;
		}
	}

	if (index == -1) {
		id = -1;
		return Shared<Process>{};
	}

	auto child = Shared<Process>::make(false);

	// copy the private portion of the address space
	for (unsigned pdi=512; pdi<1024; pdi++) {
		auto parent_pde = pd[pdi];
		if ((parent_pde & 1) == 0) continue;
		auto parent_pt = (uint32_t*) (parent_pde & 0xFFFFF000);

		auto child_pde = child->pd[pdi];
		auto child_pt = (uint32_t*) (child_pde & 0xFFFFF000);
		
		if ((child_pde & 1) == 0) {
			child_pt = (uint32_t*) PhysMem::alloc_frame();
			child->pd[pdi] = uint32_t(child_pt) | 7;
		}
		
		for (unsigned pti=0; pti<1024; pti++) {
			auto child_pte = child_pt[pti];
			if ((child_pte & 1) == 1) continue;
			auto parent_pte = parent_pt[pti];
			if ((parent_pte & 1) == 0) continue;
			auto parent_frame = parent_pte & 0xFFFFF000;
			//Debug::printf("fork: copying %x\n",(pdi << 22) | (pti << 12));
			auto child_frame = PhysMem::alloc_frame();
			//Debug::printf("fork: copying:%x, parent:%x, child:%x\n",(pdi << 22) | (pti << 12),parent_frame,child_frame);
			memcpy((void*)child_frame,(void*)parent_frame,PhysMem::FRAME_SIZE);
			child_pt[pti] = uint32_t(child_frame) | 7;
		}
	}

	//child->addressSpace->copyFrom(addressSpace);
	for (auto i = 0; i<NSEM; i++) {
		auto s = sems[i];
		if (s != nullptr) {
			child->sems[i] = s;
		}
	}
        for (auto i = 0; i<NFILE; i++) {
                child->files[i] = files[i];
        }

	children[index] = child->output;
	id = PROC | index;
	return child;
}

int Process::getSemaphoreIndex(int id) {
	int kind = id & 0xF0000000;
	int index = id & INDEX_MASK;
	if (kind != SEM) return -1;
	if (index >= NSEM) return -1;
	return index;
}

int Process::getChildIndex(int id) {
	int kind = id & 0xF0000000;
	int index = id & INDEX_MASK;
	if (kind != PROC) return -1;
	if (index >= NCHILD) return -1;
	return index;
}

int Process::getFileIndex(int id) {
	int kind = id & 0xF0000000;
	int index = id & INDEX_MASK;
	if (kind != FL) return -1;
	if (index >= NFILE) return -1;
	return index;
}

int Process::close(int id) {
	auto index = getSemaphoreIndex(id);

	if (index != -1) {
		auto sem = sems[index];
		if (sem == nullptr) {
			return -1;
		}
		sems[index] = nullptr;
		return 0;
	}

	index = getChildIndex(id);

	if (index != -1) {
		auto e = children[index];
		if (e == nullptr) {
			return -1;
		}
		children[index] = nullptr;
		return 0;
	}

	index = getFileIndex(id);

	if (index != -1) {
		auto e = files[index];
		if (e == nullptr) {
			return -1;
		}
		files[index] = nullptr;
		return 0;
	}

	return -1;
}

int Process::wait(int id, uint32_t* ptr) {
	auto index = getChildIndex(id);
	if (index < 0) return index;
	auto e = children[index];
	if (e == nullptr) return -1;
	*ptr = e->get();
	children[index] = nullptr;
	return 0;
}

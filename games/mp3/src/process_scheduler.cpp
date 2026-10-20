// Host reimplementation of Mario Party 3's HuPrc process scheduler (decomp: src/process.c).
//
// The game runs each "process" as a coroutine on its own guest stack and switches between them
// with setjmp/longjmp. Recompiled code runs on the host stack, so those jumps can't work as-is.
// Instead each process gets a Windows fiber with its own host stack and its own recomp_context;
// the scheduler loop below is a direct port of HuPrcCall, with "longjmp into a process" replaced
// by SwitchToFiber and "longjmp back to the scheduler" replaced by switching back.
//
// The functions defined here are listed in mp3.us.toml's [patches] ignored list, so the
// recompiled callers link against these instead.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include "recomp.h"
#include "ultramodern/ultramodern.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#error "process_scheduler.cpp currently only implements fibers for Windows"
#endif

// Recompiled game functions used by the scheduler.
extern "C" {
    void HuMemMemoryFreePerm(uint8_t* rdram, recomp_context* ctx);
}

namespace {

// Guest addresses (from the decomp build's marioparty3.elf).
constexpr gpr addr_processtop = (int32_t)0x800BBA8C; // top_process
constexpr gpr addr_processcur = (int32_t)0x800BBA90; // current_process
constexpr gpr addr_processcnt = (int32_t)0x800BBA94; // process_count (s16)
constexpr gpr addr_sPrcSleepLoc = (int32_t)0x800D20A8;
constexpr uint32_t vram_HuPrcExit = 0x8004EFD4;
// Written into prc_jump.func when a process yields, standing in for the return address the
// original setjmp would store. If the scheduler later finds a different value there, it was
// overwritten (HuPrcCall does this for dead processes) and the process must run that instead.
constexpr uint32_t vram_yield_marker = 0x8004F010; // HuPrcSleep

// Process struct field offsets.
constexpr int PRC_NEXT = 0x00;
constexpr int PRC_PREV = 0x04; // youngest_child: the previous process in the run list
constexpr int PRC_OLDEST_CHILD = 0x08;
constexpr int PRC_HEAP = 0x18;
constexpr int PRC_EXEC_MODE = 0x1C;
constexpr int PRC_STAT = 0x1E;
constexpr int PRC_SLEEP_TIME = 0x24;
constexpr int PRC_JUMP_SP = 0x2C;
constexpr int PRC_JUMP_FUNC = 0x30;
constexpr int PRC_DESTRUCTOR = 0x88;

constexpr uint16_t EXEC_PROCESS_DEFAULT = 0;
constexpr uint16_t EXEC_PROCESS_SLEEPING = 1;
constexpr uint16_t EXEC_PROCESS_WATCH = 2;
constexpr uint16_t EXEC_PROCESS_DEAD = 3;

// How a process handed control back to the scheduler (the original longjmp values).
enum class YieldResult { None = 0, Yielded = 1, Terminated = 2 };

constexpr SIZE_T fiber_stack_commit = 64 * 1024;
constexpr SIZE_T fiber_stack_reserve = 1024 * 1024;

struct ProcessFiber {
    LPVOID fiber = nullptr;
    gpr process = 0;
    recomp_context ctx{};
    uint8_t* rdram = nullptr;
    // Guest $sp at the last yield, also written to prc_jump.sp. A mismatch on resume means the
    // Process struct at this address was recreated, so this fiber is stale.
    uint32_t last_yield_sp = 0;
};

// All scheduler state belongs to the game thread that calls HuPrcCall.
thread_local std::unordered_map<uint32_t, ProcessFiber*> fibers;
thread_local LPVOID scheduler_fiber = nullptr;
thread_local ProcessFiber* running = nullptr;
thread_local YieldResult last_result = YieldResult::None;

[[noreturn]] void fatal(const char* msg) {
    fprintf(stderr, "[MP3] process scheduler: %s\n", msg);
    fflush(stderr);
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, msg);
    std::abort();
}

void copy_context(recomp_context& dst, const recomp_context& src) {
    dst = src;
    // f_odd points into the context itself; rebase it onto the copy.
    ptrdiff_t f_odd_offset = reinterpret_cast<const uint8_t*>(src.f_odd) - reinterpret_cast<const uint8_t*>(&src);
    dst.f_odd = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(&dst) + f_odd_offset);
}

// Returns control to the scheduler. Only valid on a process fiber.
void switch_to_scheduler(YieldResult result) {
    last_result = result;
    SwitchToFiber(scheduler_fiber);
}

// Called on the process fiber whenever it is resumed: if the scheduler replaced prc_jump.func
// (dead processes get HuPrcExit), run that function in place of returning from the yield.
void check_redirect(ProcessFiber* pf) {
    uint8_t* rdram = pf->rdram;
    uint32_t func = (uint32_t)MEM_W(PRC_JUMP_FUNC, pf->process);
    if (func != vram_yield_marker) {
        recomp_context* ctx = &pf->ctx;
        LOOKUP_FUNC(func)(rdram, ctx);
        fatal("redirected process function returned");
    }
}

void WINAPI process_fiber_main(LPVOID param) {
    ProcessFiber* pf = static_cast<ProcessFiber*>(param);
    uint8_t* rdram = pf->rdram;
    recomp_context* ctx = &pf->ctx;

    // The original longjmp starts the process with $sp = prc_jump.sp and $ra = prc_jump.func,
    // so a process function that returns jumps back to its own start. Mirror that.
    while (true) {
        uint32_t func = (uint32_t)MEM_W(PRC_JUMP_FUNC, pf->process);
        ctx->r29 = (int32_t)MEM_W(PRC_JUMP_SP, pf->process);
        ctx->r31 = (int32_t)func;
        LOOKUP_FUNC(func)(rdram, ctx);
    }
}

void delete_fiber(ProcessFiber* pf) {
    fibers.erase((uint32_t)pf->process);
    DeleteFiber(pf->fiber);
    delete pf;
}

// Runs a process until it yields or terminates (the original "longjmp(&prc_jump, 1)").
YieldResult run_process(uint8_t* rdram, recomp_context* sched_ctx, gpr process) {
    MEM_W(0, addr_processcur) = (int32_t)process;

    uint32_t key = (uint32_t)process;

    auto it = fibers.find(key);
    ProcessFiber* pf = (it != fibers.end()) ? it->second : nullptr;

    // A fiber whose saved stack pointer no longer matches belongs to an older process that
    // reused this address (processes freed without terminating, e.g. on a heap reset).
    if (pf != nullptr && (uint32_t)MEM_W(PRC_JUMP_SP, process) != pf->last_yield_sp) {
        delete_fiber(pf);
        pf = nullptr;
    }

    if (pf == nullptr) {
        pf = new ProcessFiber();
        pf->process = process;
        pf->rdram = rdram;
        copy_context(pf->ctx, *sched_ctx);
        pf->fiber = CreateFiberEx(fiber_stack_commit, fiber_stack_reserve, 0, process_fiber_main, pf);
        if (pf->fiber == nullptr) {
            fatal("CreateFiberEx failed");
        }
        fibers[key] = pf;
    }

    ProcessFiber* prev_running = running;
    running = pf;
    last_result = YieldResult::None;
    SwitchToFiber(pf->fiber);
    running = prev_running;

    YieldResult result = last_result;
    if (result == YieldResult::Terminated) {
        delete_fiber(pf);
    }
    return result;
}

// Common body of HuPrcSleep and HuPrcChildWait: the original
// "if (!setjmp(&process->prc_jump)) longjmp(&process_jmp_buf, 1);".
void yield_current(uint8_t* rdram, recomp_context* ctx) {
    ProcessFiber* pf = running;
    if (pf == nullptr || GetCurrentFiber() != pf->fiber) {
        fatal("process yield called outside of a process");
    }
    pf->last_yield_sp = (uint32_t)ctx->r29;
    MEM_W(PRC_JUMP_SP, pf->process) = (int32_t)ctx->r29;
    MEM_W(PRC_JUMP_FUNC, pf->process) = (int32_t)vram_yield_marker;
    switch_to_scheduler(YieldResult::Yielded);
    check_redirect(pf);
}

} // namespace

// void HuPrcCall(s32 time)
extern "C" void HuPrcCall(uint8_t* rdram, recomp_context* ctx) {
    int32_t time = (int32_t)ctx->r4;

    if (!IsThreadAFiber()) {
        if (ConvertThreadToFiber(nullptr) == nullptr) {
            fatal("ConvertThreadToFiber failed");
        }
    }
    LPVOID prev_scheduler = scheduler_fiber;
    scheduler_fiber = GetCurrentFiber();

    gpr cur = (int32_t)MEM_W(0, addr_processtop);
    while (cur != 0) {
        uint16_t exec_mode = MEM_HU(PRC_EXEC_MODE, cur);

        // sPrcSleepLoc = process->prc_jump.sp[10] (debug info: where the process last slept)
        MEM_W(0, addr_sPrcSleepLoc) = MEM_W(40, (int32_t)MEM_W(PRC_JUMP_SP, cur));

        if ((MEM_HU(PRC_STAT, cur) & 0x1) && exec_mode != EXEC_PROCESS_DEAD) {
            cur = (int32_t)MEM_W(PRC_NEXT, cur);
            continue;
        }

        bool run = false;
        switch (exec_mode) {
            case EXEC_PROCESS_SLEEPING: {
                int32_t sleep_time = MEM_W(PRC_SLEEP_TIME, cur);
                if (sleep_time > 0) {
                    sleep_time -= time;
                    MEM_W(PRC_SLEEP_TIME, cur) = sleep_time;
                    if (sleep_time <= 0) {
                        MEM_W(PRC_SLEEP_TIME, cur) = 0;
                        MEM_H(PRC_EXEC_MODE, cur) = EXEC_PROCESS_DEFAULT;
                    }
                }
                break;
            }
            case EXEC_PROCESS_WATCH:
                if (MEM_W(PRC_OLDEST_CHILD, cur) == 0) {
                    MEM_H(PRC_EXEC_MODE, cur) = EXEC_PROCESS_DEFAULT;
                    run = true;
                }
                break;
            case EXEC_PROCESS_DEAD:
                MEM_W(PRC_JUMP_FUNC, cur) = (int32_t)vram_HuPrcExit;
                run = true;
                break;
            case EXEC_PROCESS_DEFAULT:
                run = true;
                break;
        }

        if (!run) {
            cur = (int32_t)MEM_W(PRC_NEXT, cur);
            continue;
        }

        YieldResult result = run_process(rdram, ctx, cur);
        gpr next = (int32_t)MEM_W(PRC_NEXT, cur);
        if (result == YieldResult::Terminated) {
            // HuMemMemoryFreePerm(process->heap)
            ctx->r4 = (int32_t)MEM_W(PRC_HEAP, cur);
            HuMemMemoryFreePerm(rdram, ctx);
        }
        else if (result != YieldResult::Yielded) {
            fatal("process returned to the scheduler without yielding");
        }
        cur = next;
    }

    MEM_W(0, addr_processcur) = 0;
    scheduler_fiber = prev_scheduler;
}

// void HuPrcSleep(s32 time)
extern "C" void HuPrcSleep(uint8_t* rdram, recomp_context* ctx) {
    gpr process = (int32_t)MEM_W(0, addr_processcur);
    int32_t time = (int32_t)ctx->r4;
    if (time != 0 && MEM_HU(PRC_EXEC_MODE, process) != EXEC_PROCESS_DEAD) {
        MEM_H(PRC_EXEC_MODE, process) = EXEC_PROCESS_SLEEPING;
        MEM_W(PRC_SLEEP_TIME, process) = time;
    }
    yield_current(rdram, ctx);
}

// void HuPrcChildWait(void)
extern "C" void HuPrcChildWait(uint8_t* rdram, recomp_context* ctx) {
    gpr process = (int32_t)MEM_W(0, addr_processcur);
    if (MEM_W(PRC_OLDEST_CHILD, process) != 0) {
        MEM_H(PRC_EXEC_MODE, process) = EXEC_PROCESS_WATCH;
        yield_current(rdram, ctx);
    }
}

// void HuPrcTerminate(Process* process)
extern "C" void HuPrcTerminate(uint8_t* rdram, recomp_context* ctx) {
    gpr process = (int32_t)ctx->r4;

    uint32_t destructor = (uint32_t)MEM_W(PRC_DESTRUCTOR, process);
    if (destructor != 0) {
        LOOKUP_FUNC(destructor)(rdram, ctx);
    }

    // UnlinkProcess(&top_process, process) (a static function in the decomp, inlined here)
    gpr next = (int32_t)MEM_W(PRC_NEXT, process);
    gpr prev = (int32_t)MEM_W(PRC_PREV, process);
    if (next != 0) {
        MEM_W(PRC_PREV, next) = (int32_t)prev;
    }
    if (prev != 0) {
        MEM_W(PRC_NEXT, prev) = (int32_t)next;
    }
    else {
        MEM_W(0, addr_processtop) = (int32_t)next;
    }
    MEM_H(0, addr_processcnt) = MEM_H(0, addr_processcnt) - 1; // s16

    ProcessFiber* pf = running;
    if (pf == nullptr || GetCurrentFiber() != pf->fiber) {
        fatal("HuPrcTerminate called outside of a process");
    }
    switch_to_scheduler(YieldResult::Terminated);
    fatal("terminated process was resumed");
}

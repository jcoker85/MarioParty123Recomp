// Implementations for libultra functions that N64Recomp leaves to the runtime by name
// (its ignored_funcs list) but that N64ModernRuntime doesn't provide.

#include <cstdio>
#include <cstdlib>
#include "recomp.h"
#include "ultramodern/ultramodern.hpp"

[[noreturn]] static void unimplemented(const char* name) {
    fprintf(stderr, "[MP2] unimplemented runtime function called: %s\n", name);
    fflush(stderr);
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, name);
    std::abort();
}

// Used only by the HuPrc process scheduler (src/game/process.c), which switches between
// coroutine stacks with setjmp/longjmp. That needs a host-side scheduler; until then, stop here.
extern "C" void setjmp_recomp(uint8_t* rdram, recomp_context* ctx) {
    unimplemented("setjmp (HuPrc coroutine scheduler not reimplemented yet)");
}

// s32 osPfsIsPlug(OSMesgQueue* queue, u8* pattern): report no controller paks inserted.
extern "C" void osPfsIsPlug_recomp(uint8_t* rdram, recomp_context* ctx) {
    MEM_B(0, ctx->r5) = 0;
    ctx->r2 = 0;
}

// 64DD (libleo) entry points. Mario Party probes for the disk drive; report it absent.
extern "C" void osLeoDiskInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

extern "C" void osEPiWriteIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

// libultra osViGetNextFramebuffer (unnamed in the decomp, listed in mp2.us.toml's ignored list).
// The game's framebuffer picker uses it to avoid drawing into the buffer that is about to be shown.
extern "C" void osViGetNextFramebuffer_recomp(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_800A6A70_A7670(uint8_t* rdram, recomp_context* ctx) {
    osViGetNextFramebuffer_recomp(rdram, ctx);
}

// Thread-queue internals, called only from libleo. Should never run on cartridge.
extern "C" void __osPopThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    unimplemented("__osPopThread");
}

extern "C" void __osEnqueueThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    unimplemented("__osEnqueueThread");
}


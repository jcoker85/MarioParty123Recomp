// Implementations for libultra functions that N64Recomp leaves to the runtime by name
// (its ignored_funcs list) but that N64ModernRuntime doesn't provide.

#include <cstdio>
#include <cstdlib>
#include "recomp.h"
#include "ultramodern/ultramodern.hpp"
#include "librecomp/game.hpp"
#include "librecomp/addresses.hpp"

[[noreturn]] static void unimplemented(const char* name) {
    fprintf(stderr, "[MP3] unimplemented runtime function called: %s\n", name);
    fflush(stderr);
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, name);
    std::abort();
}

// Used only by the HuPrc process scheduler (src/process.c), which switches between
// coroutine stacks with setjmp/longjmp. That needs a host-side scheduler; until then, stop here.
extern "C" void setjmp_recomp(uint8_t* rdram, recomp_context* ctx) {
    unimplemented("setjmp (HuPrc coroutine scheduler not reimplemented yet)");
}

// s32 osPfsIsPlug(OSMesgQueue* queue, u8* pattern): report no controller paks inserted.
extern "C" void osPfsIsPlug_recomp(uint8_t* rdram, recomp_context* ctx) {
    MEM_B(0, ctx->r5) = 0;
    ctx->r2 = 0;
}

// libultra osViSetSpecialFeatures (unnamed in the decomp, listed in mp3.us.toml's ignored list).
// The recompiled original only edits guest VI state the runtime never reads, which left gamma on
// and the picture washed out.
extern "C" void osViSetSpecialFeatures_recomp(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_8007D740_main(uint8_t* rdram, recomp_context* ctx) {
    osViSetSpecialFeatures_recomp(rdram, ctx);
}

// 64DD (libleo) entry points. Mario Party may probe for the disk drive; report it absent.
extern "C" void osLeoDiskInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

extern "C" void osEPiWriteIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}

// Thread-queue internals, called only from libleo. Should never run on cartridge.
extern "C" void __osPopThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    unimplemented("__osPopThread");
}

extern "C" void __osEnqueueThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    unimplemented("__osEnqueueThread");
}


// s32 osPiReadIo(u32 devAddr, u32* data). Only called by the KMC dev-board debug printf
// (kmc_proutSyncPrintf), which polls bit 2 of a debugger port until it reads "ready". Cart ROM
// reads go through the runtime; anything else reports all bits set so that loop never spins.
extern "C" void osPiReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    uint32_t physical_addr = (uint32_t)ctx->r4 & 0x1FFFFFFF;
    gpr data_addr = ctx->r5;
    if (physical_addr >= recomp::rom_base && physical_addr - recomp::rom_base + 4 <= recomp::get_rom().size()) {
        recomp::do_rom_pio(rdram, data_addr, physical_addr);
    }
    else {
        MEM_W(0, data_addr) = (int32_t)0xFFFFFFFF;
    }
    ctx->r2 = 0;
}

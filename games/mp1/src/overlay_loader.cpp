// Host version of Mario Party's OvlLoad (decomp: src/engine/ovlman.c) that also tells the
// runtime which recompiled overlay is now resident, so calls into overlay code resolve.
// Listed in mp1.us.toml's [patches] ignored list.

#include "recomp.h"

extern "C" {
    void HuRomDmaCodeRead(uint8_t* rdram, recomp_context* ctx);
    void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);
    void unload_overlays(int32_t ram_addr, uint32_t size);
}

namespace {
    // Guest addresses (from the decomp build's marioparty.elf).
    constexpr gpr addr_overlay_table = (int32_t)0x800C1C74;
    constexpr gpr addr_D_800C1C70 = (int32_t)0x800C1C70;

    // Every overlay in overlay_table loads at 0x800F65E0 and the largest ends at 0x80110490,
    // so only one is resident at a time. The unload range also starts lower, at the end of the
    // main segment (0x800CCE50): at boot the runtime registers every section in the first 1MB
    // of ROM as if it were part of the main segment, which includes the first few overlays at
    // bogus addresses between there and 0x800FE970. They are dropped on the first load.
    constexpr int32_t overlay_unload_start = (int32_t)0x800CCE50;
    constexpr uint32_t overlay_unload_size = 0x80110490 - 0x800CCE50;

    // OverlayInfo field offsets (sizeof 0x24).
    constexpr int OVL_SIZE = 0x24;
    constexpr int OVL_ROM_START = 0x00;
    constexpr int OVL_ROM_END = 0x04;
    constexpr int OVL_RAM_START = 0x08;
    constexpr int OVL_BSS_START = 0x1C;
    constexpr int OVL_BSS_END = 0x20;
}

// void OvlLoad(s32 overlayIndex)
extern "C" void OvlLoad(uint8_t* rdram, recomp_context* ctx) {
    int32_t index = (int32_t)ctx->r4;
    gpr entry = addr_overlay_table + index * OVL_SIZE;

    uint32_t rom_start = (uint32_t)MEM_W(OVL_ROM_START, entry);
    uint32_t rom_end = (uint32_t)MEM_W(OVL_ROM_END, entry);
    gpr ram_start = (int32_t)MEM_W(OVL_RAM_START, entry);
    gpr bss_start = (int32_t)MEM_W(OVL_BSS_START, entry);
    gpr bss_end = (int32_t)MEM_W(OVL_BSS_END, entry);

    // HuRomDmaCodeRead(rom_start, ram_start, rom_end - rom_start)
    ctx->r4 = (int32_t)rom_start;
    ctx->r5 = ram_start;
    ctx->r6 = (int32_t)(rom_end - rom_start);
    HuRomDmaCodeRead(rdram, ctx);

    unload_overlays(overlay_unload_start, overlay_unload_size);
    load_overlays(rom_start, (int32_t)ram_start, rom_end - rom_start);

    for (gpr addr = bss_start; addr < bss_end; addr++) {
        MEM_B(0, addr) = 0;
    }

    MEM_B(0, addr_D_800C1C70) = 0;
}

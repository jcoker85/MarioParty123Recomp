// Host version of Mario Party 2's OvlLoad (decomp: src/util.c) that also tells the runtime which
// recompiled overlay is now resident, so calls into overlay code resolve.
// Listed in mp2.us.toml's [patches] ignored list.

#include "recomp.h"

extern "C" {
    void HuRomDmaCodeRead(uint8_t* rdram, recomp_context* ctx);
    void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);
    void unload_overlays(int32_t ram_addr, uint32_t size);
}

namespace {
    // Guest addresses (from the decomp build's marioparty2.elf).
    constexpr gpr addr_overlay_table = (int32_t)0x800C8874; // _ovltbl
    constexpr gpr addr_D_800C8870 = (int32_t)0x800C8870;

    // Every overlay in _ovltbl loads at 0x80102800 (the end of the main segment) and the largest
    // ends at 0x801157E0, so only one is resident at a time. At boot the runtime registers every
    // section in the first 1MB of ROM as if it were part of the main segment, which maps the
    // first few overlays (ROM 0xD57F0-0x100000) linearly from 0x80102800 up to 0x8012D010.
    // Those bogus registrations are dropped on the first load, so the range covers both.
    constexpr int32_t overlay_unload_start = (int32_t)0x80102800;
    constexpr uint32_t overlay_unload_size = 0x8012D010 - 0x80102800;

    // OverlayTable field offsets (sizeof 0x24).
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

    MEM_B(0, addr_D_800C8870) = 0;
}

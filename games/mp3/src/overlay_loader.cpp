// Host versions of Mario Party 3's overlay loaders (decomp: src/util.c) that also tell the runtime
// which recompiled overlay is now resident, so calls into overlay code resolve.
// Both are listed in mp3.us.toml's [patches] ignored list.
//
// MP3 has two overlay regions:
// - _ovltbl (OvlLoad): 128 scene overlays (menus, minigames, boards), all at 0x801059A0. The
//   largest ends (bss included) at 0x801208A0, so only one is resident at a time.
// - _modeovltbl (func_8000B1A0_main): 3 "mode" overlays loaded alongside the scene overlay
//   (minigame, name_81, shared_board), all at 0x800D6B70 (the end of main's bss).
//
// At boot the runtime registers every section in the first 1MB of ROM as if it were part of the
// main segment. That maps the minigame and name_81 mode overlays (ROM 0xA96F0-0xEA790) linearly
// from 0x800A8AF0 (the end of .main) up to 0x800E9B90; the mode-overlay unload range starts at
// 0x800A8AF0 so the first mode load drops those bogus registrations.

#include "recomp.h"

extern "C" {
    void dmaReadOvl(uint8_t* rdram, recomp_context* ctx);
    void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);
    void unload_overlays(int32_t ram_addr, uint32_t size);
}

namespace {
    // Guest addresses (from the decomp build's marioparty3.elf).
    constexpr gpr addr_ovltbl = (int32_t)0x800962F4;     // _ovltbl
    constexpr gpr addr_modeovltbl = (int32_t)0x800974F4; // _modeovltbl
    constexpr gpr addr_D_800962F0_main = (int32_t)0x800962F0;

    constexpr int32_t scene_unload_start = (int32_t)0x801059A0;
    constexpr uint32_t scene_unload_size = 0x801208A0 - 0x801059A0;
    // Ends at the end of name_81's code+data (the largest mode section), which stays below the
    // scene overlay region: unload_overlays treats a section starting at the range end as overlapping.
    constexpr int32_t mode_unload_start = (int32_t)0x800A8AF0;
    constexpr uint32_t mode_unload_size = 0x801029C0 - 0x800A8AF0;

    // OverlayTable field offsets (sizeof 0x24).
    constexpr int OVL_SIZE = 0x24;
    constexpr int OVL_ROM_START = 0x00;
    constexpr int OVL_ROM_END = 0x04;
    constexpr int OVL_VRAM_START = 0x08;
    constexpr int OVL_TEXT_START = 0x0C;
    constexpr int OVL_TEXT_END = 0x10;
    constexpr int OVL_DATA_START = 0x14;
    constexpr int OVL_DATA_END = 0x18;
    constexpr int OVL_BSS_START = 0x1C;
    constexpr int OVL_BSS_END = 0x20;

    constexpr uint8_t HAS_TEXT_SECTION = 1;
    constexpr uint8_t HAS_DATA_SECTION = 2;
    constexpr uint8_t HAS_BSS_SECTION = 4;

    // dmaReadOvl(rom, ram, size)
    void dma_read(uint8_t* rdram, recomp_context* ctx, uint32_t rom, gpr ram, uint32_t size) {
        ctx->r4 = (int32_t)rom;
        ctx->r5 = ram;
        ctx->r6 = (int32_t)size;
        dmaReadOvl(rdram, ctx);
    }

    void clear(uint8_t* rdram, gpr start, gpr end) {
        for (gpr addr = start; addr < end; addr++) {
            MEM_B(0, addr) = 0;
        }
    }
}

// void OvlLoad(s32 overlayIndex)
extern "C" void OvlLoad(uint8_t* rdram, recomp_context* ctx) {
    int32_t index = (int32_t)ctx->r4;
    gpr entry = addr_ovltbl + index * OVL_SIZE;

    uint32_t rom_start = (uint32_t)MEM_W(OVL_ROM_START, entry);
    uint32_t rom_end = (uint32_t)MEM_W(OVL_ROM_END, entry);
    gpr vram_start = (int32_t)MEM_W(OVL_VRAM_START, entry);
    gpr bss_start = (int32_t)MEM_W(OVL_BSS_START, entry);
    gpr bss_end = (int32_t)MEM_W(OVL_BSS_END, entry);

    dma_read(rdram, ctx, rom_start, vram_start, rom_end - rom_start);

    unload_overlays(scene_unload_start, scene_unload_size);
    load_overlays(rom_start, (int32_t)vram_start, rom_end - rom_start);

    clear(rdram, bss_start, bss_end);

    MEM_B(0, addr_D_800962F0_main) = 0;
}

// void func_8000B1A0_main(s32 index, u8 sectionFlags)
// Called with flags 7 to switch mode overlays, or 6 to reset the resident one's data and bss.
extern "C" void func_8000B1A0_main(uint8_t* rdram, recomp_context* ctx) {
    int32_t index = (int32_t)ctx->r4;
    uint8_t flags = (uint8_t)ctx->r5;
    gpr entry = addr_modeovltbl + index * OVL_SIZE;

    uint32_t rom_start = (uint32_t)MEM_W(OVL_ROM_START, entry);
    uint32_t rom_end = (uint32_t)MEM_W(OVL_ROM_END, entry);
    gpr vram_start = (int32_t)MEM_W(OVL_VRAM_START, entry);
    gpr text_start = (int32_t)MEM_W(OVL_TEXT_START, entry);
    gpr text_end = (int32_t)MEM_W(OVL_TEXT_END, entry);
    gpr data_start = (int32_t)MEM_W(OVL_DATA_START, entry);
    gpr data_end = (int32_t)MEM_W(OVL_DATA_END, entry);
    gpr bss_start = (int32_t)MEM_W(OVL_BSS_START, entry);
    gpr bss_end = (int32_t)MEM_W(OVL_BSS_END, entry);

    uint32_t text_size = (uint32_t)(text_end - text_start);

    if (flags & HAS_TEXT_SECTION) {
        dma_read(rdram, ctx, rom_start, vram_start, text_size);
        unload_overlays(mode_unload_start, mode_unload_size);
        load_overlays(rom_start, (int32_t)vram_start, rom_end - rom_start);
    }
    if (flags & HAS_DATA_SECTION) {
        dma_read(rdram, ctx, rom_start + text_size, data_start, (uint32_t)(data_end - data_start));
    }
    if (flags & HAS_BSS_SECTION) {
        clear(rdram, bss_start, bss_end);
    }
}

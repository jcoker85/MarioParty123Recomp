// Widescreen fixes: emit rt64 extended GBI commands (lib/rt64/include/rt64_extended_gbi.h) into
// the game's display lists around 2D drawing that was authored for a 4:3 screen.
// These functions are called from hooks in mp1.us.toml.

#include <bit>
#include "recomp.h"

namespace {
    // Gfx* the game appends its main display list through (D_800F37DC).
    constexpr gpr addr_gfx_head = (int32_t)0x800F37DC;

    // Values from rt64_extended_gbi.h for an F3DEX2 microcode (the ROM ships F3DEX2 2.06 variants).
    constexpr uint32_t RT64_HOOK_OPCODE = 0xE0;         // G_SPNOOP on F3DEX2
    constexpr uint32_t RT64_HOOK_MAGIC_NUMBER = 0x525464;
    constexpr uint32_t RT64_HOOK_OP_ENABLE = 0x1;
    constexpr uint32_t RT64_EXTENDED_OPCODE = 0x64;
    constexpr uint32_t G_EX_SETVIEWPORTALIGN_V1 = 0x07;
    constexpr uint32_t G_EX_SETRECTASPECT_V1 = 0x33;
    constexpr uint32_t G_EX_SETSCISSORASPECT_V1 = 0x34;
    constexpr uint32_t G_EX_ORIGIN_NONE = 0x800;
    constexpr uint32_t G_EX_ORIGIN_CENTER = 0x200;

    // Camera array (D_800C3110, 6 entries of 0x4F8 bytes). Viewport scale and translate are
    // Vec3f at 0x24 and 0x30, in viewport units (the full-screen default is 640, 480 for both).
    constexpr gpr addr_cameras = (int32_t)0x800C3110;
    constexpr int32_t CAMERA_SIZE = 0x4F8;
    constexpr int CAMERA_VP_SCALE = 0x24;
    constexpr int CAMERA_VP_TRANS = 0x30;

    // Appends a command through the Gfx* stored at guest address gfx_ptr_addr.
    void append_command_at(uint8_t* rdram, gpr gfx_ptr_addr, uint32_t w0, uint32_t w1) {
        gpr head = (int32_t)MEM_W(0, gfx_ptr_addr);
        MEM_W(0, head) = (int32_t)w0;
        MEM_W(4, head) = (int32_t)w1;
        MEM_W(0, gfx_ptr_addr) = (int32_t)(head + 8);
    }

    void append_command(uint8_t* rdram, uint32_t w0, uint32_t w1) {
        append_command_at(rdram, addr_gfx_head, w0, w1);
    }

    void append_enable(uint8_t* rdram, gpr gfx_ptr_addr) {
        append_command_at(rdram, gfx_ptr_addr, (RT64_HOOK_OPCODE << 24) | RT64_HOOK_MAGIC_NUMBER, (RT64_HOOK_OP_ENABLE << 28) | RT64_EXTENDED_OPCODE);
    }
}

// Called at the start of the camera setup, func_8001D658(s16 index, Gfx** gfx), before it sets the
// camera's viewport. Cameras with the standard full-screen viewport stay on rt64's widescreen
// path. Cameras whose viewport is shifted or zoomed (the board map scrolls by moving an oversized
// viewport over a 2D background) are aligned to G_EX_ORIGIN_CENTER instead, which renders them
// exactly as on a 4:3 screen, centered.
//
// Without this, rt64 decides per RSP task whether to widen a viewport, from how its scissor
// compares to the rest of the task. The board's 3D is split across tasks: characters are widened
// (and the off-center viewport gets shifted), while shadows and space markers in the task that
// also draws full-screen passes are not, so they drifted apart from each other and the board.
extern "C" void mp1_camera_viewport_align(uint8_t* rdram, gpr gfx_ptr_addr, int32_t camera_index) {
    gpr camera = (int32_t)MEM_W(0, addr_cameras) + camera_index * CAMERA_SIZE;
    float scale_x = std::bit_cast<float>((uint32_t)MEM_W(CAMERA_VP_SCALE, camera));
    float scale_y = std::bit_cast<float>((uint32_t)MEM_W(CAMERA_VP_SCALE + 4, camera));
    float trans_x = std::bit_cast<float>((uint32_t)MEM_W(CAMERA_VP_TRANS, camera));
    float trans_y = std::bit_cast<float>((uint32_t)MEM_W(CAMERA_VP_TRANS + 4, camera));
    bool full_screen = (scale_x == 640.0f) && (scale_y == 480.0f) && (trans_x == 640.0f) && (trans_y == 480.0f);

    append_enable(rdram, gfx_ptr_addr);
    // gEXSetViewportAlign(origin, 0, 0)
    append_command_at(rdram, gfx_ptr_addr, (RT64_EXTENDED_OPCODE << 24) | G_EX_SETVIEWPORTALIGN_V1, full_screen ? G_EX_ORIGIN_NONE : G_EX_ORIGIN_CENTER);
    append_command_at(rdram, gfx_ptr_addr, 0, 0);
}

// Sets rt64's rect and scissor aspect mode (G_EX_ASPECT_AUTO = 0, G_EX_ASPECT_STRETCH = 1).
// Extended commands are disabled again at the end of every task, so enable them each time.
extern "C" void mp1_set_2d_aspect(uint8_t* rdram, uint32_t aspect) {
    append_command(rdram, (RT64_HOOK_OPCODE << 24) | RT64_HOOK_MAGIC_NUMBER, (RT64_HOOK_OP_ENABLE << 28) | RT64_EXTENDED_OPCODE);
    append_command(rdram, (RT64_EXTENDED_OPCODE << 24) | G_EX_SETRECTASPECT_V1, aspect & 0x3);
    append_command(rdram, (RT64_EXTENDED_OPCODE << 24) | G_EX_SETSCISSORASPECT_V1, aspect & 0x3);
}

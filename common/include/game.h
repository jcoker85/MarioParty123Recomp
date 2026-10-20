#ifndef __MP_GAME_H__
#define __MP_GAME_H__

// What makes each executable a particular game. games/mpN/src/game_info.cpp defines mp::game and
// mp::register_overlays; everything under common/ reads them.

#include <cstdint>

#include "librecomp/game.hpp"
#include "librecomp/rsp.hpp"

namespace mp {
    struct GameInfo {
        const char* log_tag;            // prefix for stderr messages, e.g. "[MP3]"
        const char* window_title;
        const char* program_name;
        uint64_t rom_hash;              // xxh3 of the US ROM
        const char* internal_name;
        const char* display_name;
        const char8_t* game_id;         // stored ROM and save file name
        const char* mod_game_id;        // --game argument and mod manifest id
        recomp::SaveType save_type;
        RspUcodeFunc* audio_ucode;      // RSPRecomp output for the game's audio microcode
    };

    extern const GameInfo game;

    // All three games use one app data folder (%LOCALAPPDATA%\MarioParty123Recomp, or the working
    // directory when it holds portable.txt). Settings (general/graphics/sound/controls.json) live
    // at its root and are shared; each game keeps its ROM, saves and mods in <root>/<mod_game_id>/.
    inline constexpr const char8_t* program_id = u8"MarioParty123Recomp";

    // Defined per game (games/mpN/src/register_overlays.cpp), from the generated overlay tables.
    void register_overlays();

    // Shared (common/src/config.cpp).
    void init_config();
}

#endif

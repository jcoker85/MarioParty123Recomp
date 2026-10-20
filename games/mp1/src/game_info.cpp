#include "game.h"

extern RspUcodeFunc aspMain;

const mp::GameInfo mp::game = {
    .log_tag = "[MP1]",
    .window_title = "Mario Party: Recompiled",
    .program_name = "Mario Party: Recompiled",
    .rom_hash = 0x19F905A0CBC9FE8FULL,
    .internal_name = "Mario Party",
    .display_name = "Mario Party",
    .game_id = u8"mario_party",
    .mod_game_id = "mp1",
    .save_type = recomp::SaveType::Eep4k,
    .audio_ucode = aspMain,
};

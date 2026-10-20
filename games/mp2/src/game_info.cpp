#include "game.h"

extern RspUcodeFunc aspMain;

const mp::GameInfo mp::game = {
    .log_tag = "[MP2]",
    .window_title = "Mario Party 2: Recompiled",
    .program_name = "Mario Party 2: Recompiled",
    .rom_hash = 0xD2DBFB3B4D7A5E81ULL,
    .internal_name = "MarioParty2",
    .display_name = "Mario Party 2",
    .game_id = u8"mario_party_2",
    .mod_game_id = "mp2",
    .save_type = recomp::SaveType::Eep4k,
    .audio_ucode = aspMain,
};

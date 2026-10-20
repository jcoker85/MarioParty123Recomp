#include "game.h"

extern RspUcodeFunc n_aspMain;

const mp::GameInfo mp::game = {
    .log_tag = "[MP3]",
    .window_title = "Mario Party 3: Recompiled",
    .program_name = "Mario Party 3: Recompiled",
    .rom_hash = 0x39F865D6EC68E629ULL,
    .internal_name = "MarioParty3",
    .display_name = "Mario Party 3",
    .game_id = u8"mario_party_3",
    .mod_game_id = "mp3",
    .save_type = recomp::SaveType::Eep16k,
    .audio_ucode = n_aspMain,
};

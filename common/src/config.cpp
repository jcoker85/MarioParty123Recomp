#include "game.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "recompinput/recompinput.h"
#include "util/file.h"
#include <filesystem>

static void set_control_descriptions() {
    using recompinput::GameInput;
    recompinput::set_game_input_description(GameInput::Y_AXIS_POS, "Move Down / Menu Down");
    recompinput::set_game_input_description(GameInput::Y_AXIS_NEG, "Move Up / Menu Up");
    recompinput::set_game_input_description(GameInput::X_AXIS_NEG, "Move Left / Menu Left");
    recompinput::set_game_input_description(GameInput::X_AXIS_POS, "Move Right / Menu Right");
    recompinput::set_game_input_description(GameInput::A, "Confirm / Hit Dice Block");
    recompinput::set_game_input_description(GameInput::B, "Cancel");
    recompinput::set_game_input_description(GameInput::START, "Pause");
}

// Copies default-settings/*.json (shipped next to the executables) into the shared settings folder
// when that file doesn't exist there yet. Options a default file leaves out keep the runtime's
// defaults. Like assets/, the folder is found relative to the working directory.
static void install_default_settings(const std::filesystem::path& settings_dir) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("default-settings", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        std::filesystem::path target = settings_dir / entry.path().filename();
        if (!std::filesystem::exists(target)) {
            std::filesystem::copy_file(entry.path(), target, ec);
        }
    }
}

void mp::init_config() {
    std::filesystem::path settings_dir = recomp::get_settings_path();
    std::filesystem::path game_dir = recomp::get_config_path();
    if (!game_dir.empty()) {
        std::filesystem::create_directories(game_dir);
    }
    install_default_settings(settings_dir);

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    general_options.has_mouse_sensitivity = false;
    recompui::config::create_general_tab(general_options);

    recompui::config::create_graphics_tab();

    set_control_descriptions();
    recompui::config::create_controls_tab();

    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();

    recompui::config::finalize();
}

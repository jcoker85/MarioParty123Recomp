// Debug tool: scripted controller input for driving the game in tests without window focus.
// Write a file named "input.cmd" in the working directory; it is read, deleted and played back
// on controller 1, one step per line:
//   A 4            hold A for 4 input polls (the game polls about once per frame)
//   START+DU 2     hold several buttons at once
//   STICK 0 -80 10 hold the analog stick at (x, y) in N64 units (-80..80) for 10 polls
//   - 30           no input for 30 polls
// Lines starting with # are ignored. While a script plays, it replaces controller 1's real input.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

#include "recompinput/profiles.h"
#include "test_input.hpp"
#include "game.h"

namespace {

struct Step {
    uint16_t buttons = 0;
    float x = 0.0f;
    float y = 0.0f;
    int polls = 1;
};

std::mutex script_mutex;
std::deque<Step> script;
uint32_t polls_since_check = 0;

uint16_t button_bit(const std::string& name) {
    static const struct { const char* name; uint16_t bit; } bits[] = {
        {"A", 0x8000}, {"B", 0x4000}, {"Z", 0x2000}, {"START", 0x1000},
        {"DU", 0x0800}, {"DD", 0x0400}, {"DL", 0x0200}, {"DR", 0x0100},
        {"L", 0x0020}, {"R", 0x0010},
        {"CU", 0x0008}, {"CD", 0x0004}, {"CL", 0x0002}, {"CR", 0x0001},
    };
    for (const auto& b : bits) {
        if (name == b.name) {
            return b.bit;
        }
    }
    fprintf(stderr, "%s input.cmd: unknown button %s\n", mp::game.log_tag, name.c_str());
    return 0;
}

void load_script() {
    const std::filesystem::path path = "input.cmd";
    if (!std::filesystem::exists(path)) {
        return;
    }
    std::ifstream in(path);
    std::string line;
    size_t loaded = 0;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string what;
        if (!(ls >> what) || what[0] == '#') {
            continue;
        }
        Step step{};
        if (what == "STICK") {
            ls >> step.x >> step.y;
        }
        else if (what != "-") {
            std::stringstream names(what);
            std::string name;
            while (std::getline(names, name, '+')) {
                step.buttons |= button_bit(name);
            }
        }
        ls >> step.polls;
        if (step.polls > 0) {
            script.push_back(step);
            loaded++;
        }
    }
    in.close();
    std::error_code ec;
    std::filesystem::remove(path, ec);
    fprintf(stderr, "%s input.cmd: loaded %zu steps\n", mp::game.log_tag, loaded);
}

} // namespace

bool mp::get_input_with_script(int player_index, uint16_t* buttons_out, float* x_out, float* y_out) {
    if (player_index == 0) {
        std::lock_guard lock{ script_mutex };
        if (script.empty() && ++polls_since_check >= 15) {
            polls_since_check = 0;
            load_script();
        }
        if (!script.empty()) {
            Step& step = script.front();
            *buttons_out = step.buttons;
            // get_input reports the stick normalized to -1..1; the runtime scales it back to N64 units.
            *x_out = step.x / 80.0f;
            *y_out = step.y / 80.0f;
            if (--step.polls <= 0) {
                script.pop_front();
            }
            return true;
        }
    }
    return recompinput::profiles::get_n64_input(player_index, buttons_out, x_out, y_out);
}

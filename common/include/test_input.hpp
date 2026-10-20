#ifndef __MP_TEST_INPUT_HPP__
#define __MP_TEST_INPUT_HPP__

#include <cstdint>

namespace mp {
    // Controller input callback that plays back an "input.cmd" script when one is present
    // (see test_input.cpp), falling back to the configured input profile otherwise.
    bool get_input_with_script(int player_index, uint16_t* buttons_out, float* x_out, float* y_out);
}

#endif

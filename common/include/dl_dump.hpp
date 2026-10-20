#ifndef __MP_DL_DUMP_HPP__
#define __MP_DL_DUMP_HPP__

#include <memory>
#include "ultramodern/renderer_context.hpp"

namespace mp {
    // Wraps the renderer so a frame's display list can be dumped on request (see dl_dump.cpp).
    std::unique_ptr<ultramodern::renderer::RendererContext> wrap_with_dl_dumper(
        std::unique_ptr<ultramodern::renderer::RendererContext> inner, uint8_t* rdram);
}

#endif

// Debug tool: dumps a frame's display list as text. Create a file named "dldump.request" in the
// working directory; the next 8 graphics tasks are written to dldump_<n>.txt (one file per task) and
// the request is removed. Decodes the F3DEX2 commands relevant to layout work (matrices, viewports, scissors,
// rectangles, display list calls); everything else is printed raw.

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "ultramodern/ultra64.h"
#include "ultramodern/renderer_context.hpp"
#include "dl_dump.hpp"
#include "game.h"

namespace {

class Dumper {
public:
    Dumper(const uint8_t* rdram, FILE* out) : rdram(rdram), out(out) {}

    void dump(uint32_t addr) { walk(addr, 0); }

private:
    const uint8_t* rdram;
    FILE* out;
    uint32_t segments[16] = {};
    size_t lines = 0;
    static constexpr size_t max_lines = 200000;

    uint32_t resolve(uint32_t addr) const {
        return (segments[(addr >> 24) & 0xF] + (addr & 0x00FFFFFF)) & 0x007FFFFF;
    }
    uint32_t word(uint32_t phys) const {
        return *reinterpret_cast<const uint32_t*>(rdram + phys);
    }
    int16_t half(uint32_t phys, int index) const {
        uint32_t w = word(phys + (index / 2) * 4);
        return (int16_t)((index & 1) ? (w & 0xFFFF) : (w >> 16));
    }

    void print(int depth, const char* fmt, ...) {
        if (lines++ >= max_lines) {
            return;
        }
        fprintf(out, "%*s", depth * 2, "");
        va_list args;
        va_start(args, fmt);
        vfprintf(out, fmt, args);
        va_end(args);
        fputc('\n', out);
    }

    void walk(uint32_t addr, int depth) {
        if (depth > 16) {
            print(depth, "(max depth)");
            return;
        }
        uint32_t pc = resolve(addr);
        for (int i = 0; i < 100000 && lines < max_lines; i++, pc += 8) {
            uint32_t w0 = word(pc), w1 = word(pc + 4);
            uint8_t op = w0 >> 24;
            switch (op) {
                case 0xDA: { // G_MTX (params are xor'd with G_MTX_PUSH)
                    uint32_t params = (w0 & 0xFF) ^ 0x01;
                    uint32_t m = resolve(w1);
                    // Fixed point 4x4: integer halves then fractional halves.
                    auto el = [&](int r, int c) {
                        int idx = r * 4 + c;
                        int32_t ip = half(m, idx);
                        uint16_t fp = (uint16_t)half(m + 32, idx);
                        return (float)((ip << 16) | fp) / 65536.0f;
                    };
                    print(depth, "MTX %s%s%s @%08X  [%.3f %.3f %.3f %.3f] [%.3f %.3f %.3f %.3f] [.. .. %.3f %.3f] [%.3f %.3f %.3f %.3f]",
                        (params & 4) ? "PROJ" : "MV", (params & 2) ? " LOAD" : " MUL", (params & 1) ? " PUSH" : "", w1,
                        el(0, 0), el(0, 1), el(0, 2), el(0, 3), el(1, 0), el(1, 1), el(1, 2), el(1, 3), el(2, 2), el(2, 3),
                        el(3, 0), el(3, 1), el(3, 2), el(3, 3));
                    break;
                }
                case 0xD8: print(depth, "POPMTX %08X", w1); break;
                case 0xDC: { // G_MOVEMEM
                    uint32_t index = w0 & 0xFF;
                    if (index == 0x08) {
                        uint32_t v = resolve(w1);
                        print(depth, "VIEWPORT scale(%.1f,%.1f) trans(%.1f,%.1f)  => x %.1f..%.1f y %.1f..%.1f",
                            half(v, 0) / 4.0f, half(v, 1) / 4.0f, half(v, 4) / 4.0f, half(v, 5) / 4.0f,
                            (half(v, 4) - half(v, 0)) / 4.0f, (half(v, 4) + half(v, 0)) / 4.0f,
                            (half(v, 5) - half(v, 1)) / 4.0f, (half(v, 5) + half(v, 1)) / 4.0f);
                    }
                    else {
                        print(depth, "MOVEMEM idx=%02X @%08X", index, w1);
                    }
                    break;
                }
                case 0x01: print(depth, "VTX n=%u @%08X", (w0 >> 12) & 0xFF, w1); break;
                case 0x05: print(depth, "TRI1"); break;
                case 0x06: print(depth, "TRI2"); break;
                case 0x07: print(depth, "QUAD"); break;
                case 0xDE: {
                    bool branch = ((w0 >> 16) & 0xFF) == 1;
                    print(depth, "%s @%08X", branch ? "BRANCH_DL" : "DL", w1);
                    walk(w1, depth + 1);
                    if (branch) {
                        return;
                    }
                    break;
                }
                case 0xDF: print(depth, "ENDDL"); return;
                case 0xDB: { // G_MOVEWORD
                    uint32_t index = (w0 >> 16) & 0xFF;
                    uint32_t offset = w0 & 0xFFFF;
                    if (index == 0x06) {
                        segments[(offset / 4) & 0xF] = w1 & 0x00FFFFFF;
                        print(depth, "SEGMENT %u = %08X", (offset / 4) & 0xF, w1);
                    }
                    else {
                        print(depth, "MOVEWORD idx=%02X off=%04X %08X", index, offset, w1);
                    }
                    break;
                }
                case 0xE4: case 0xE5: {
                    uint32_t h1 = word(pc + 12), h2 = word(pc + 20);
                    print(depth, "%s (%.2f,%.2f)-(%.2f,%.2f) tile=%u  st=%08X d=%08X", op == 0xE4 ? "TEXRECT" : "TEXRECTFLIP",
                        ((w1 >> 12) & 0xFFF) / 4.0f, (w1 & 0xFFF) / 4.0f, ((w0 >> 12) & 0xFFF) / 4.0f, (w0 & 0xFFF) / 4.0f,
                        (w1 >> 24) & 7, h1, h2);
                    break;
                }
                case 0xED: print(depth, "SCISSOR (%.2f,%.2f)-(%.2f,%.2f)", ((w0 >> 12) & 0xFFF) / 4.0f, (w0 & 0xFFF) / 4.0f,
                    ((w1 >> 12) & 0xFFF) / 4.0f, (w1 & 0xFFF) / 4.0f); break;
                case 0xF6: print(depth, "FILLRECT (%u,%u)-(%u,%u)", ((w1 >> 12) & 0xFFF) / 4, (w1 & 0xFFF) / 4,
                    ((w0 >> 12) & 0xFFF) / 4, (w0 & 0xFFF) / 4); break;
                case 0xFF: print(depth, "SETCIMG width=%u @%08X", (w0 & 0xFFF) + 1, w1); break;
                case 0xFD: print(depth, "SETTIMG fmt=%u siz=%u width=%u @%08X", (w0 >> 21) & 7, (w0 >> 19) & 3, (w0 & 0xFFF) + 1, w1); break;
                case 0xFC: print(depth, "SETCOMBINE %08X %08X", w0, w1); break;
                case 0xFA: print(depth, "PRIMCOLOR %08X", w1); break;
                case 0xE2: case 0xE3: print(depth, "%s %08X %08X", op == 0xE2 ? "SETOTHERMODE_L" : "SETOTHERMODE_H", w0, w1); break;
                case 0xD9: print(depth, "GEOMETRYMODE clear=%06X set=%08X", w0 & 0xFFFFFF, w1); break;
                case 0xE1: case 0xF1: break; // texrect continuation words
                case 0xE7: case 0xE6: case 0xE8: case 0xE9: break; // syncs
                default: print(depth, "%02X %08X %08X", op, w0, w1); break;
            }
        }
    }
};

class DumpingRendererContext : public ultramodern::renderer::RendererContext {
public:
    DumpingRendererContext(std::unique_ptr<ultramodern::renderer::RendererContext> inner, uint8_t* rdram)
        : inner(std::move(inner)), rdram(rdram) {
        setup_result = this->inner->get_setup_result();
        chosen_api = this->inner->get_chosen_api();
    }

    bool valid() override { return inner->valid(); }
    ultramodern::renderer::SetupResult get_setup_result() const override { return inner->get_setup_result(); }
    ultramodern::renderer::GraphicsApi get_chosen_api() const override { return inner->get_chosen_api(); }
    bool update_config(const ultramodern::renderer::GraphicsConfig& o, const ultramodern::renderer::GraphicsConfig& n) override { return inner->update_config(o, n); }
    void enable_instant_present() override { inner->enable_instant_present(); }
    void send_dummy_workload(uint32_t fb_address) override { inner->send_dummy_workload(fb_address); }
    void update_screen() override {
        if (frame_stats) {
            uint32_t origin = ultramodern::renderer::get_vi_regs()->VI_ORIGIN_REG;
            if (origin != last_origin) {
                const ultramodern::renderer::ViRegs* vi = ultramodern::renderer::get_vi_regs();
                fprintf(stderr, "%s frame origin=%06X vis=%u tasks=%u status=%X width=%u hstart=%X vstart=%X xscale=%X yscale=%X fbs:%s\n", mp::game.log_tag, origin, vis_since_change, tasks_since_change,
                    vi->VI_STATUS_REG, vi->VI_WIDTH_REG, vi->VI_H_START_REG, vi->VI_V_START_REG, vi->VI_X_SCALE_REG, vi->VI_Y_SCALE_REG, task_fbs.c_str());
                last_origin = origin;
                vis_since_change = 0;
                tasks_since_change = 0;
                task_fbs.clear();
            }
            vis_since_change++;
        }
        inner->update_screen();
    }
    void shutdown() override { inner->shutdown(); }
    uint32_t get_display_framerate() const override { return inner->get_display_framerate(); }
    float get_resolution_scale() const override { return inner->get_resolution_scale(); }

    void send_dl(const OSTask* task) override {
        if (tasks_to_dump == 0 && (++frame_counter % 15) == 0 && std::filesystem::exists("dldump.request")) {
            std::error_code ec;
            std::filesystem::remove("dldump.request", ec);
            tasks_to_dump = 8;
        }
        if (tasks_to_dump > 0) {
            tasks_to_dump--;
            std::string name = "dldump_" + std::to_string(dump_index++) + ".txt";
            if (FILE* out = fopen(name.c_str(), "w")) {
                fprintf(out, "task type=%u data_ptr=%08X size=%u\n", task->t.type, (uint32_t)task->t.data_ptr, task->t.data_size);
                Dumper(rdram, out).dump((uint32_t)task->t.data_ptr);
                fclose(out);
                fprintf(stderr, "%s wrote %s\n", mp::game.log_tag, name.c_str());
            }
        }
        if (frame_stats) {
            // Each Hudson gfx task starts with gSPSegment(fb segment, framebuffer) then a branch.
            uint32_t phys = (uint32_t)task->t.data_ptr & 0x007FFFFF;
            uint32_t w0 = *reinterpret_cast<const uint32_t*>(rdram + phys);
            uint32_t w1 = *reinterpret_cast<const uint32_t*>(rdram + phys + 4);
            char buf[32];
            snprintf(buf, sizeof(buf), " %06X", (w0 >> 24) == 0xDB ? (w1 & 0x00FFFFFF) : 0xFFFFFF);
            task_fbs += buf;
            tasks_since_change++;
        }
        inner->send_dl(task);
    }

private:
    std::unique_ptr<ultramodern::renderer::RendererContext> inner;
    uint8_t* rdram;
    uint32_t frame_counter = 0;
    int dump_index = 0;
    int tasks_to_dump = 0;
    // MP_FRAME_STATS=1: log gfx tasks (and their target framebuffers) per displayed VI origin.
    bool frame_stats = getenv("MP_FRAME_STATS") != nullptr;
    uint32_t last_origin = 0;
    uint32_t vis_since_change = 0;
    uint32_t tasks_since_change = 0;
    std::string task_fbs;
};

} // namespace

std::unique_ptr<ultramodern::renderer::RendererContext> mp::wrap_with_dl_dumper(
    std::unique_ptr<ultramodern::renderer::RendererContext> inner, uint8_t* rdram) {
    return std::make_unique<DumpingRendererContext>(std::move(inner), rdram);
}

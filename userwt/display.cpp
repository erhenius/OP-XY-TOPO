#include "display.h"
#include "table.h"
#include <opxy/sample_overview.hpp>

namespace userwt {
using namespace opxy;
void display_reset(DisplayState& state) {
    state.ready = state.changed = state.surface.rows = 0;
}

void display(DisplayState& state, const draw::Input& input, Frame& frame) {
    using namespace draw;
    const SampleOverview& sample = *opxy_sample_overview();
    const u32 bits = width_bits(input.parameters[0]);
    const char* const widths[] = {"256", "512", "1024", "2048"};
    header(frame, 0, widths[bits - 8]);
    const bool valid = sample.assigned && sample.loaded && sample.begin < sample.end &&
                       sample.end <= sample.frames && usable(sample.end - sample.begin, bits);
    const u32 position = input.parameters[1] > 0 ? u32(input.parameters[1]) >> 16 : 0;
    const u32 warp = input.parameters[2] > 0 ? u32(input.parameters[2]) >> 16 : 0;
    const bool same = valid && state.ready && state.track == input.track &&
                      state.generation == sample.generation && state.begin == sample.begin &&
                      state.end == sample.end && state.bits == bits && state.reverse == sample.reverse;
    if (!same) state.ready = 0;
    state.changed = 0;
    if (valid && (!same || state.warp != warp) &&
        opxy_userwt_surface(input.track, bits, warp, &state.surface)) {
        state.track = input.track;
        state.generation = sample.generation;
        state.begin = sample.begin;
        state.end = sample.end;
        state.bits = bits;
        state.reverse = sample.reverse;
        state.warp = warp;
        state.ready = state.changed = 1;
    }
    const u32 rows = state.ready ? state.surface.rows : 0;
    const u32 selected = rows ? (position * (rows - 1) + 16383) / 32767 : 0;
    for (u32 i = 0; i < surface_rows; ++i) {
        if (i >= rows) {
            hide(frame, i);
            continue;
        }
        const i32 depth = rows > 1 ? i32(i * 1024 / (rows - 1)) : 512;
        const u32 shade = i == selected ? 3 : depth < 342 ? 2 : depth < 684 ? 1 : 0;
        place(frame, i, 42 * unit + 108 * depth, 177 * unit - 95 * depth, unit, shade, stroke);
    }
    const char* message = !sample.assigned ? "Choose a sample" : !sample.loaded ? "Loading" :
                          !valid ? "Check cycle length" : !rows ? "Reading table" : "";
    text(frame, 0, 40 * unit, 132 * unit, unit, message);
    if (valid) place(frame, surface_rows, (40 + (position * 400 + 16383) / 32767) * unit,
                     32 * unit, unit, 0, keep);
    else hide(frame, surface_rows);
}
}

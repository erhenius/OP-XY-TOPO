#pragma once
#include <opxy/draw.hpp>
#include <opxy/wavetable_surface.hpp>

namespace userwt {
struct DisplayState {
    opxy::u32 ready, changed, track, generation, begin, end, bits, reverse, warp;
    opxy::draw::TableSurface surface;
};
using Frame = opxy::draw::Frame<0,0,opxy::draw::surface_rows + 1,1,4>;
inline const opxy::draw::TableSurface* changed_table_surface(const DisplayState& state) {
    return state.changed ? &state.surface : nullptr;
}
}

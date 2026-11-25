#pragma once
#include <opxy/userwt_cycle.hpp>

namespace userwt {
enum Mode { Clean, Splice, Sync, Both };
inline Mode mode_of(opxy::i32 control) {
    return control > 0 ? static_cast<Mode>(opxy::u32(control) >> 29) : Clean;
}

// Reflect the second read head at the far end of the table.
// At fractional positions its interpolation direction reflects as well.
struct Pair { opxy::u32 wave, next; };
inline Pair alternate_pair(opxy::u32 wave, opxy::u32 last, opxy::u32 amount) {
    const opxy::u32 half = (last >> 1) + (last & 1);
    const opxy::u32 travel = (half >> 15) * amount +
                             (((half & 32767) * amount + 16384) >> 15);
    const opxy::u32 target = wave + travel;
    if (target < last) return {target, target + 1};
    const opxy::u32 reflected = last - (target - last);
    return {reflected, reflected ? reflected - 1 : 0};
}
}

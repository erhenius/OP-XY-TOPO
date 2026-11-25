#include <opxy/sample_engine.hpp>
#include <opxy/q31.hpp>
#include "table.h"

namespace userwt {
using namespace opxy;
struct Voice { SampleView sample; u32 phase, drift_phase, lap; };

void initialize(Voice& v, const SampleView& sample) {
    v.sample = sample;
    v.phase = v.drift_phase = v.lap = 0;
}
void start(Voice& v) { v.phase = v.drift_phase = v.lap = 0; }

// Both endpoints are 16-bit amplitudes. Q15 weighting fits a signed word.
static inline i32 mix(i32 a, i32 b, u32 fraction) {
    return a + ((b - a) * i32(fraction) >> 15);
}
static inline i32 at_phase(const i16* wave, u32 stride, u32 index,
                           u32 next, u32 fraction) {
    return mix(wave[index * stride], wave[next * stride], fraction);
}

// The block dispatcher selects a compiled loop. Clean has no alternate bank,
// sync arithmetic or cycle-parity tracking in its per-sample path.
template<Mode mode>
static void render_mode(Voice& v, const SampleBlock& block) {
    const bool splice = mode == Splice || mode == Both;
    const bool sync = mode == Sync || mode == Both;
    const i32* p = block.parameters;
    const u32 bits = width_bits(p[0]), width = 1u << bits;
    const u32 begin = block.region.begin, end = block.region.end;
    const u32 stride = v.sample.channels;
    if (!v.sample.data || (stride != 1 && stride != 2) || end > v.sample.frames ||
        begin >= end || !usable(end - begin, bits)) {
        for (u32 i = 0; i < block_frames; ++i) block.output[i] = 0;
        return;
    }
    const u32 last = ((end - begin) >> bits) - 1;
    const u32 pos = p[1] > 0 ? u32(p[1]) >> 16 : 0;
    const u32 scaled = pos == 32767u ? last << 15 : last * pos;
    const u32 wave = scaled >> 15, blend = scaled & 32767u;
    const i16* first = v.sample.data + (begin + wave * width) * stride;
    const i16* second = wave < last ? first + width * stride : first;
    const u32 amount = p[2] > 0 ? u32(p[2]) >> 16 : 0;
    const u32 ratio = 65536u + amount * 13u;
    const u32 whole = ratio >> 16, frac = ratio & 65535u;
    const i16* banks[2] = {first, first};
    const i16* next_banks[2] = {second, second};
    if (splice) {
        const Pair alt = alternate_pair(wave, last, amount);
        banks[1] = v.sample.data + (begin + alt.wave * width) * stride;
        next_banks[1] = v.sample.data + (begin + alt.next * width) * stride;
    }
    const u32 drift = p[3] > 0 ? u32(p[3]) >> 16 : 0;
    // A slow triangle centered at concert pitch, at most about 7 cents.
    const u32 angle = (v.drift_phase + 0x40000000u) >> 16;
    const i32 triangle = i32(angle < 32768 ? angle : 65535u - angle) - 16384;
    const i32 detune = (triangle * i32(drift)) >> 22;
    v.drift_phase += 1814239u;  // 0.291 Hz at 64 frames per block
    const i32 gain = i32(block.region.level > sample_unity_level ? sample_unity_level : block.region.level);
    for (u32 i = 0; i < block_frames; ++i) {
        const u32 phase = block.region.reverse ? 0u - v.phase : v.phase;
        u32 read_phase = phase;
        if (mode == Clean) {
            const u32 tent = phase < 0x80000000u ? phase : 0u - phase;
            read_phase += (tent >> 16) * amount;
        } else if (sync) {
            read_phase = phase * whole + (phase >> 16) * frac;
        }
        const u32 index = read_phase >> (32u - bits), next = (index + 1u) & (width - 1u);
        const u32 fraction = (read_phase >> (17u - bits)) & 32767u;
        const u32 bank = splice ? v.lap : 0u;
        i32 value = at_phase(banks[bank], stride, index, next, fraction);
        if (blend) value = mix(value, at_phase(next_banks[bank], stride, index, next, fraction), blend);
        // Half-scale at unity leaves headroom for polyphony.
        block.output[i] = gain == i32(sample_unity_level) ? value * 32768 :
            opxy::q31::multiply(value * 65536, gain * 8);
        const u32 step = block.pitch[i];
        const u32 previous = v.phase;
        v.phase += step + u32(i32(step >> 15) * detune);
        if (splice) v.lap ^= u32(v.phase < previous);
    }
}

void render_sample(Voice& v, const SampleBlock& block) {
    switch (mode_of(block.parameters[4])) {
    case Clean:  return render_mode<Clean>(v, block);
    case Splice: return render_mode<Splice>(v, block);
    case Sync:   return render_mode<Sync>(v, block);
    case Both:   return render_mode<Both>(v, block);
    }
}
}

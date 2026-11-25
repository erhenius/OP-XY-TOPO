#include <cassert>
#include <cstdio>
#include <vector>
#include "wavetable_surface_cache.hpp"
#include "sample.cpp"

int main() {
    using namespace opxy;
    using namespace opxy::draw;
    platform::SurfaceCache cache{};
    TableSurface surface{};
    unsigned cases = 0;
    for (const u32 channels : {1u, 2u})
    for (const u32 bits : {8u, 9u, 10u, 11u})
    for (const u32 count : {1u, 2u, 8u, 32u, 33u, 97u})
    for (const bool rev : {false, true})
    for (const u32 bend : {0u, 16123u, 32767u}) {
        const u32 width = 1u << bits, begin = 7, end = begin + width * count;
        std::vector<i16> pcm((end + 9) * channels, -32768);
        for (u32 i = begin; i < end; ++i)
            pcm[i * channels] = i16((i * 32713u + (i / width) * 7919u) & 65535);
        SampleView view{pcm.data(), end + 9, 44100, channels};
        assert(cache.want(2, 17, ++cases, begin, end-begin, bits, bend, rev));
        assert(!cache.copy(surface));
        cache.prepare(1,17,cases,view);
        assert(!cache.copy(surface));
        cache.prepare(2,17,cases,view);
        assert(cache.copy(surface));
        assert(!cache.want(2,17,cases,begin,end-begin,bits,bend,rev));
        assert(surface.rows == (count < 32 ? count : 32));
        u32 last = 0;
        for (u32 row = 0; row < surface.rows; ++row) {
            const u32 cycle = surface_cycle(row,surface.rows,count);
            assert(row ? cycle > last : cycle == 0);
            if (row+1 == surface.rows) assert(cycle == count-1);
            last = cycle;
            // Isolate that real source period and clock one full phase per 32 samples.
            userwt::Voice voice{};
            userwt::initialize(voice,view);
            i32 params[8] = {i32((bits - 8) << 29),0,i32(bend << 16),0};
            i32 out[64]; u32 pitch[64];
            for (auto& step : pitch) step = 1u << 27;
            SampleBlock block{};
            block.output=out; block.pitch=pitch; block.parameters=params;
            block.region.begin=begin+cycle*width; block.region.end=block.region.begin+width;
            block.region.reverse=rev; block.region.level=sample_unity_level;
            userwt::render_sample(voice,block);
            for (u32 i=0;i<=surface_intervals;++i)
                assert(out[i] == i32(surface.heights[row][i])*32768);
        }
    }
    std::printf("%u bank, width, stereo, warp and reverse combinations match the oscillator.\n",cases);
}

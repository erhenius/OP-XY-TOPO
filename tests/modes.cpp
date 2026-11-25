// An eight-cycle bank with constant values identifies the selected cycle exactly.
#include "sample.cpp"
#include <cassert>
#include <initializer_list>

using namespace opxy;

int main() {
    const u32 width = 256, count = 8;
    i16 pcm[width * count];
    for (u32 i = 0; i < width * count; ++i) pcm[i] = i16((i / width) * 1000);
    i32 params[8] = {}, out[64];
    u32 pitch[64];
    for (u32& step : pitch) step = 1u << 30;
    SampleBlock block{};
    block.parameters = params;
    block.pitch = pitch;
    block.output = out;
    block.region.end = width * count;
    block.region.level = sample_unity_level;
    userwt::Voice voice{};
    userwt::initialize(voice, {pcm, width * count, 44100, 1});
    for (int mode = 0; mode < 4; ++mode) {
        for (bool reverse : {false, true}) {
            userwt::start(voice);
            params[4] = mode << 29;
            params[2] = 0x7fffffff;
            params[1] = 0x7fffffff;
            block.region.reverse = reverse;
            userwt::render_sample(voice, block);
            for (u32 i = 0; i < 64; ++i) {
                const bool alternate = (mode == 1 || mode == 3) && (i / 4) % 2;
                assert(out[i] == (alternate ? 3000 : 7000) * 32768);
            }
        }
    }
    // A one-cycle table at every mode/depth/position endpoint.
    block.region.end = width;
    for (int m = 0; m < 4; ++m) {
        params[4] = m << 29;
        for (auto depth : {0, 0x7fffffff}) for (auto pos : {0, 0x7fffffff}) {
            params[1] = pos; params[2] = depth;
            userwt::render_sample(voice, block);
            for (auto sample : out) assert(sample == 0);
        }
    }
    return 0;
}

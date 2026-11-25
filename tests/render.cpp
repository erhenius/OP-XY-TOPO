// Offline adapter: PCM is decoded from each file by offline.py.
// Feed it through the SDK's stock sample-object reader before rendering.
#include "sample.cpp"
#include "stock_sample.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>

struct File {
    std::vector<opxy::i16> pcm;
    opxy::SampleView view;
    File(const char* path, unsigned channels) {
        auto f = std::fopen(path, "rb");
        if (!f || std::fseek(f, 0, SEEK_END)) std::exit(2);
        const long bytes = std::ftell(f);
        if (bytes <= 0 || bytes % (2 * channels) || std::fseek(f, 0, SEEK_SET)) std::exit(3);
        pcm.resize(bytes / 2);
        if (std::fread(pcm.data(), 2, pcm.size(), f) != pcm.size()) std::exit(4);
        std::fclose(f);
        // Construct the fields consumed by the actual SDK adapter.
        alignas(void*) unsigned char obj[64] = {};
        const opxy::u32 present = 1, frames = pcm.size()/channels, rate = 44100;
        const auto data = pcm.data();
        std::memcpy(obj, &present, 4);
        std::memcpy(obj + 8, &data, sizeof(data));
        std::memcpy(obj + 0x30, &frames, 4);
        std::memcpy(obj + 0x38, &rate, 4);
        obj[0x3c] = channels == 2;
        view = opxy::platform::stock_sample_view(obj);
        if (view.frames != frames || view.data != data || view.channels != channels) std::exit(5);
    }
};

int main(int argc, char** argv) {
    if (argc != 8) return 1;
    File file(argv[1], std::atoi(argv[2]));
    const unsigned width = std::atoi(argv[3]);
    opxy::i32 params[8] = {width == 2048 ? 0x7fffffff : width == 1024 ? (1<<30) :
                           width == 512 ? (1<<29) : 0, std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6])};
    opxy::u32 pitch[64];
    // Step exactly one source index per frame; a complete period visits every sample.
    for (auto& step : pitch) step = (0x80000000u / width) * 2u;
    userwt::Voice voice{};
    userwt::initialize(voice,file.view);
    userwt::start(voice);
    opxy::i32 output[64];
    opxy::SampleBlock b{};
    b.parameters=params; b.pitch=pitch; b.output=output; b.output_channels=1;
    b.region.end=file.view.frames; b.region.level=opxy::sample_unity_level;
    auto out=std::fopen(argv[7],"wb");
    if (!out) return 6;
    for (unsigned n=0; n<width*2; n+=64) {
        userwt::render_sample(voice,b);
        if (std::fwrite(output,4,64,out)!=64) return 7;
    }
    return std::fclose(out)!=0;
}

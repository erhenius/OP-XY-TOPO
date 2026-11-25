#include "sample.cpp"
#include "display.cpp"

using namespace opxy;
static draw::SampleOverview overview{};
extern "C" const draw::SampleOverview* opxy_sample_overview() { return &overview; }

static bool available;
static unsigned reads;
extern "C" bool opxy_userwt_surface(u32, u32, u32, draw::TableSurface* out) {
    ++reads;
    if (!available) return false;
    out->rows=8;
    return true;
}

int main() {
    i16 first[256], second[256];
    for (unsigned i = 0; i < 256; ++i) {
        first[i] = i16(i * 100 - 15000);
        second[i] = i16(12000 - int(i) * 90);
    }
    i32 parameters[8] = {}, output[64];
    u32 pitch[64];
    for (unsigned i=0; i<64; ++i) pitch[i] = 1u << 24;
    SampleBlock block{};
    block.parameters=parameters; block.pitch=pitch; block.output=output;
    block.region.end=256; block.region.level=sample_unity_level;
    userwt::Voice a{}, b{};
    userwt::initialize(a, SampleView{first, 256, 44100, 1});
    userwt::start(a);
    userwt::render_sample(a,block);
    userwt::initialize(b, SampleView{second, 256, 44100, 1});
    userwt::start(b);
    userwt::render_sample(b,block);
    for (unsigned i=0; i<64; ++i) if (output[i]!=second[i]*32768) return 1;
    userwt::render_sample(a,block);
    for (unsigned i=0; i<64; ++i) if (output[i]!=first[i+64]*32768) return 2;
    block.region.level=sample_unity_level/2;
    userwt::start(b);
    userwt::render_sample(b,block);
    if (output[0]!=second[0]*16384) return 3;
    userwt::initialize(b, SampleView{nullptr,0,0,1});
    userwt::render_sample(b,block);
    for (unsigned i=0; i<64; ++i) if (output[i]) return 4;
    userwt::DisplayState state{};
    userwt::Frame frame{};
    draw::Input input{};
    userwt::display_reset(state);
    for (unsigned i=0; i<4; ++i) {
        overview.assigned=i>0; overview.loaded=i>1; overview.frames=2048; overview.end=i==3?2048:19;
        userwt::display(state,input,frame);
        if (!draw::valid(frame)) return 5;
    }
    available=true;
    userwt::display(state,input,frame);
    const unsigned initial=reads;
    if (!userwt::changed_table_surface(state) || !draw::valid(frame)) return 6;
    for (unsigned i=0;i<1000;++i) {
        input.parameters[1]=i%2 ? 0x7fffffff : 0;
        userwt::display(state,input,frame);
        if (!state.ready || reads!=initial || userwt::changed_table_surface(state)) return 7;
        if (frame.nodes[i%2 ? 7 : 0].shade!=3 || frame.nodes[8].x!=draw::hidden_x*draw::unit)
            return 8;
    }
    available=false;
    ++overview.generation;
    userwt::display(state,input,frame);
    if (state.ready || frame.nodes[0].x!=draw::hidden_x*draw::unit) return 9;
    available=true;
    userwt::display(state,input,frame);
    if (!userwt::changed_table_surface(state) || !draw::valid(frame)) return 10;
    userwt::display_reset(state);
    if (userwt::changed_table_surface(state) || state.ready) return 11;
    return 0;
}

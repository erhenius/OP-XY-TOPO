// Build next to a copy of the native source and the host stock.hpp shim.
// All stock stand-ins have the same signatures as the native calls.
#include "userwt_selector.cpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

const char userwt_engine_id[] __asm__("userwt_engine_id") = {0};
static bool owned = true, ready_engine = true;
static u32 screen = 4, track = 0, unhandled, starts, stops, acquisitions, releases;
static u32 received_key;
static std::vector<u8> packet;
static i32 k1;
static Vector params{0,&k1,&k1+1};
static bool cache_loaded = true;
static Window original;
static void check(bool pass,const char* what) {
    if (!pass) { std::cerr << what << '\n'; std::exit(1); }
}
int userwt_ready() { return ready_engine; }
static u32 send_to_audio(u32 a,u32 b,const u8* m,u32 n,u32 c,u32 d,u32 e) {
    check(a == 1 && b == 0xa4 && c == 0 && d == 255 && e == 255,"transport ABI");
    packet.assign(m,m+n); return 123;
}
static const Vector* get_vector(u32 t) { check(t==track,"track"); return &params; }
static void passthrough(const u8*,u32);
static const void* lookup(u32 manager,u32 key) {
    check(manager == 7 && received_key == key,"cache key");
    if (!cache_loaded) return nullptr;
    ++acquisitions;
    return &original;
}
static bool start_requested, stop_requested;
static void start(void*,u32 key) { ++starts; received_key=key; start_requested=true; }
static void stop(void*) { ++stops; stop_requested=true; }
static void passthrough(const u8* m,u32 n) {
    if(n==1 && m[0]==15) stop(nullptr);
    else if(n==5 && m[0]==14) start(nullptr,u32(m[1]) | u32(m[2])<<8 | u32(m[3])<<16 | u32(m[4])<<24);
    else ++unhandled;
}
static void retire(Player* p) { if(p->object) ++releases; p->object=nullptr; }
static unsigned quantum = 64;
static void render(void* state,i32* out) {
    auto* p = static_cast<Player*>(state);
    if(stop_requested) { retire(p); start_requested=stop_requested=false; }
    if(start_requested) {
        retire(p); p->key=received_key; p->position=0;
        p->object=sdk_userwt_preview_acquire(7,p->key);
        start_requested=!p->object;
    }
    const auto* sample=static_cast<const Window*>(p->object);
    for(unsigned i=0;sample && i<quantum;++i) {
        if(p->position >= sample->frames) { retire(p);break; }
        out[i]=sample->data[(1+sample->stereo)*p->position++];
    }
}
namespace opxy { namespace platform {
u32 byte(u32 a) { check(a==kSelectedTrack,"byte address");return track; }
u32 word(u32 a) { check(a==kScreen,"screen address");return screen; }
u32 track_engine(u32 t) { check(t==track,"selected track");return owned?u32(reinterpret_cast<Address>(userwt_engine_id)):2; }
Address target(u32 a) {
    switch(a) {
    case 0x11a00ec0:return reinterpret_cast<Address>(&send_to_audio);
    case 0x8d4d72a8:return reinterpret_cast<Address>(&get_vector);
    case 0x8d509d70:return reinterpret_cast<Address>(&lookup);
    case 0x8d4d83b8:return reinterpret_cast<Address>(&passthrough);
    case 0x8d50c73e:return reinterpret_cast<Address>(&start);
    case 0x8d50c72c:return reinterpret_cast<Address>(&stop);
    case 0x8d50c7ac:return reinterpret_cast<Address>(&render);
    default:std::abort();
    }
}
}}
static void send(u32 key) {
    const u8 msg[]={14,u8(key),u8(key>>8),u8(key>>16),u8(key>>24)};
    check(sdk_userwt_preview_send(1,0xa4,msg,5,0,255,255)==123,"sender return");
    sdk_userwt_preview_receive(packet.data(),packet.size());
}
static void end(Player& p) { u8 msg=15;sdk_userwt_preview_receive(&msg,1);i32 out[64]{};sdk_userwt_preview_mix(&p,out); }
int main() {
    constexpr u32 hash=0xabcd8000;
    Player p{};
    for(unsigned bits=8;bits<=11;++bits) for(u32 channels : {1u,2u}) {
        const unsigned width=1u<<bits;
        const i32 q31=(bits-8)<<29;
        k1=q31;
        std::vector<i16> pcm(width*channels*2,-25000);
        for(unsigned i=0;i<width;++i)
            pcm[i*channels]=i16(14000*std::sin(6.283185307179586*i/width));
        original={};original.occupied=1;original.data=pcm.data();original.frames=width*2;
        original.rate=44100;original.stereo=u8(channels==2);
        send(hash);
        check(packet.size()==8 && packet[7]==bits,"decoded K1");
        unsigned total=0,crossings=0; i32 prior=0;
        do {
            quantum=total%2?64:37;
            i32 out[64];std::fill(out,out+64,100000);
            sdk_userwt_preview_mix(&p,out);
            for(unsigned i=0;i<quantum && out[i]!=100000;++i) {
                unsigned phase=total*25480119u;
                unsigned idx=phase>>(32-bits), frac=(phase>>(24-bits))&255;
                i32 a=pcm[idx*channels], b=pcm[((idx+1)&(width-1))*channels];
                i32 expected=a+((b-a)*i32(frac)>>8);
                if(total>=22050-256) expected=expected*i32(22049-total)>>8;
                check(out[i]==expected,"pitched first wavelet, tail and variable block boundaries");
                if(out[i]>0 && prior<=0) ++crossings;
                prior=out[i];++total;
            }
        } while(p.object);
        check(total==22050 && crossings==131 && original.frames==width*2,"C4, duration and original file");
        quantum=64;
        for(bool wrong_track : {false,true}) {
            owned=!wrong_track;screen=wrong_track?4:2;
            send(hash);check(packet.size()==5,"unowned payload identical");
            i32 out[64]{};sdk_userwt_preview_mix(&p,out);
            check(p.object==&original && out[17]==pcm[17*channels],"normal audition");
            end(p);
        }
        owned=true;screen=4;
        cache_loaded=false;send(hash);i32 out[64]{};sdk_userwt_preview_mix(&p,out);
        check(!p.object,"wait for file");k1=0;send(hash+1);k1=q31;
        send(hash);cache_loaded=true;sdk_userwt_preview_mix(&p,out);
        check(p.object==&window && active_bits==bits && pending_key==hash,"latest highlight");
        end(p);
        original.frames=width-1;send(hash);sdk_userwt_preview_mix(&p,out);
        check(!p.object,"invalid width releases cache reference");
    }
    const u8 unknown[]={9,0};sdk_userwt_preview_receive(unknown,2);
    check(unhandled==1 && starts && stops && acquisitions==releases,"dispatch and balanced cache");
    std::cout<<"PASS: native selector with stock doubles, all four widths, cache, stop, pitch, original sampler.\n";
}

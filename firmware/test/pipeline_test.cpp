#include "pipeline_sequence.hpp"
#include "entropy/source.hpp"
#include "api/rng_json.hpp"
#include "lighting/matrix.hpp"
#include "platform.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace oml;
static bool ready=true, lose_readiness=false;
static size_t reads=0;
static bool gate() { return ready; }
uint32_t esp_random() {
    assert(reads<8); const auto word=diagnostic_test::words[reads++];
    if(lose_readiness) ready=false;
    return word;
}
int main() {
    // Exercise the actual HardwareSource wrapper, with only the silicon API
    // replaced at link time. This target cannot write experiments or device flash.
    HardwareSource source(gate); uint32_t word=123;
    ready=false; assert(!source.read(word) && reads==0 && word==123);
    ready=true; lose_readiness=true;
    assert(!source.read(word) && reads==1 && word==123); // reject transition mid-read
    lose_readiness=false; ready=true; reads=0;
    Settings s; s.starting_hue=359; s.sensitivity=.5; s.saturation=1;
    s.brightness=.04; s.smoothing_ms=200; s.accumulation_ms=100;
    Walk walk; walk.reset(s.starting_hue); Health health; RngDiagnostics d;
    Matrix matrix; matrix.init(); RenderedColor color;
    d.reset(walk,0,0); int64_t now=0;
    const int expected_net[]={4,-4,0,0}; const double expected_target[]={1,359,359,359};
    for(unsigned window=0;window<4;++window) {
        for(unsigned sample=0;sample<2;++sample) {
            assert(source.read(word)); assert(health.observe(word)); walk.sample(word);
            now+=50000; d.sample(walk,health.words,word,true,now);
        }
        const auto commit=walk.commit(s.sensitivity); d.commit(commit,health.words,++now);
        assert(commit.net==expected_net[window] && commit.samples==2);
        assert(std::abs(commit.after-expected_target[window])<1e-9);
        const double before=walk.hue(); const auto step=walk.render(100,s.smoothing_ms);
        const double delta=wrap(commit.after-before+180)-180;
        assert(std::abs(walk.hue()-wrap(before+delta*(1-std::exp(-.5))))<1e-9);
        const auto frame=color_frame(s,walk.hue(),now/1e6,word,false);
        assert(frame.walk_hue==step.after && frame.logical_hue==walk.hue());
        assert(matrix.show(frame)); color.transmitted(frame,now+200,true,&step);
        const auto bytes=wire_bytes(frame.commanded_rgb,s.rgb_order);
        assert(transmitted_bytes[0].r==bytes.r && transmitted_bytes[0].g==bytes.g && transmitted_bytes[0].b==bytes.b);
        auto* j=rng_json(d,walk,s,health,word,true,now+300);
        assert(cJSON_GetObjectItemCaseSensitive(j,"successful_samples")->valuedouble==2*(window+1));
        assert(cJSON_GetObjectItemCaseSensitive(j,"net")->valueint==net_bits(word));
        cJSON_Delete(j);
    }
    assert(health.words==8 && !health.failed());
    // Time schedules/smooths rendering but cannot move the Walk target by itself.
    const double target=walk.target();
    for(unsigned i=0;i<100;++i) { walk.render(1000000,200); assert(walk.target()==target); }
    const auto a=color_frame(s,walk.hue(),0,word,false), b=color_frame(s,walk.hue(),999999,word,false);
    assert(a.commanded_rgb.r==b.commanded_rgb.r && a.commanded_rgb.g==b.commanded_rgb.g && a.commanded_rgb.b==b.commanded_rgb.b);
    assert(walk.commit(.5).net==0 && walk.target()==target);
    diagnostic_test::KnownSequence test_source; assert(!test_source.experimental());
    // Ring overwrite is explicit and reset drops previous settings' trace history.
    for(unsigned i=0;i<200;++i) d.sample(walk,health.words,word,true,++now);
    assert(d.oldest()==d.sequence-RngDiagnostics::capacity+1);
    d.reset(walk,health.words,++now); assert(d.oldest()==d.sequence);
    TraceSnapshot snapshot; snapshot.rng=d; snapshot.walk=walk; snapshot.settings=s;
    snapshot.health=health; snapshot.color=color; snapshot.latest_word=word; snapshot.ready=true; snapshot.now_us=now+1000;
    auto* trace=trace_json(snapshot,0);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(trace,"events"))==1);
    assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(trace,"experimental_record")));
    cJSON_Delete(trace);
    std::puts("TEST ONLY: hardware gate, known +4/-4/0/0 bit windows, hue wrap/smoothing, RMT bytes and diagnostics passed; no experimental records written");
}

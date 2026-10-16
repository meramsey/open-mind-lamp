#include "lighting/color.hpp"
#include "lighting/calibration.hpp"
#include "lighting/matrix.hpp"
#include "api/color_json.hpp"
#include "storage/settings.hpp"
#include "platform.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>
using namespace oml;
static bool equal(RGB a,RGB b) { return a.r==b.r && a.g==b.g && a.b==b.b; }
static void expect_rgb(RGB actual,int r,int g,int b) { assert(equal(actual,RGB{static_cast<uint8_t>(r),static_cast<uint8_t>(g),static_cast<uint8_t>(b)})); }
static void observe(Calibration& c,TestColor seen,int64_t now=1000) {
    c.presented(c.run(),c.step()); assert(c.observe(c.run(),c.step(),seen,now));
}
int main() {
    // Primary/secondary colors + white, exact safe PWM endpoints and wraparound.
    const double hues[]={0,120,240,0,60,180,300};
    const RGB full[]={{32,0,0},{0,32,0},{0,0,32},{32,32,32},{32,32,0},{0,32,32},{32,0,32}};
    for(unsigned i=0;i<7;++i) assert(equal(hsv(hues[i],i==3?0:1,.125),full[i]));
    assert(equal(hsv(-60,1,.125),full[6])); assert(equal(hsv(720,1,.125),full[0]));
    expect_rgb(hsv(30,1,.125),32,7,0); // .5^2.2 * .125 * 255 -> 7, not 16 or 1
    expect_rgb(hsv(0,1,.04),10,0,0); expect_rgb(hsv(0,1,.08),20,0,0);
    expect_rgb(hsv(0,0,.08,.5,1,2),10,20,32); // gain then clamp, before integer conversion
    Settings s; s.gain_red=.24; assert(!valid(s)); s.gain_red=2.01; assert(!valid(s));
    s.gain_red=std::numeric_limits<double>::quiet_NaN(); assert(!valid(s)); s=Settings{};
    expect_rgb(driver_rgb({3,5,7},false),3,5,7); expect_rgb(driver_rgb({3,5,7},true),5,3,7);
    expect_rgb(wire_bytes({3,5,7},false),5,3,7); expect_rgb(wire_bytes({3,5,7},true),3,5,7);

    Matrix matrix; matrix.init(); RenderedColor state;
    for(bool order:{false,true}) {
        for(unsigned i=0;i<7;++i) {
            s.mode=Mode::Ambient; s.brightness=.125; s.starting_hue=hues[i];
            s.saturation=i==3?0:1; s.rgb_order=order;
            const auto primary=color_frame(s,73,0,0,false);
            assert(equal(primary.commanded_rgb,full[i])); assert(matrix.show(primary));
            state.transmitted(primary,9000,true);
            cJSON* encoded=color_json(state,9500);
            const auto* a=cJSON_GetObjectItemCaseSensitive(encoded,"commanded_rgb");
            const auto* bytes=cJSON_GetObjectItemCaseSensitive(encoded,"wire_bytes");
            const auto physical=transmitted_bytes[0];
            assert(cJSON_GetArrayItem(a,0)->valueint==full[i].r);
            assert(cJSON_GetArrayItem(a,1)->valueint==full[i].g);
            assert(cJSON_GetArrayItem(a,2)->valueint==full[i].b);
            assert(cJSON_GetArrayItem(bytes,0)->valueint==physical.r);
            assert(cJSON_GetArrayItem(bytes,1)->valueint==physical.g);
            assert(cJSON_GetArrayItem(bytes,2)->valueint==physical.b);
            cJSON_Delete(encoded);
        }
        s.brightness=.08;
        s.mode=Mode::Ambient; s.starting_hue=30; s.saturation=1; s.rgb_order=order;
        auto frame=color_frame(s,240,0,0,false);
        assert(frame.logical_hue==30); expect_rgb(frame.logical_rgb,255,128,0);
        assert(matrix.show(frame)); state.transmitted(frame,10000,true);
        assert(equal(transmitted_bytes[0],wire_bytes(frame.commanded_rgb,order)));
        cJSON* j=color_json(state,13000);
        const auto* commanded=cJSON_GetObjectItemCaseSensitive(j,"commanded_rgb");
        assert(cJSON_GetArrayItem(commanded,0)->valueint==frame.commanded_rgb.r);
        assert(cJSON_GetArrayItem(commanded,1)->valueint==frame.commanded_rgb.g);
        assert(cJSON_GetObjectItemCaseSensitive(j,"age_ms")->valuedouble==3);
        cJSON_Delete(j);
    }
    // API/state never publishes a failed transmission as current.
    const auto old_id=state.frame_id; fail_refresh=true;
    auto failed=color_frame(s,0,0,0,false); assert(!matrix.show(failed)); state.transmitted(failed,20000,false);
    assert(!state.valid && state.frame_id==old_id && state.rendered_at_us==10000); fail_refresh=false;
    s=Settings{}; s.mode=Mode::Breathing;
    auto high=color_frame(s,120,1.25,0,false), low=color_frame(s,120,3.75,0,false);
    assert(high.commanded_rgb.g>low.commanded_rgb.g && high.logical_hue==low.logical_hue);
    Walk walk; walk.reset(359); walk.sample(0xffffffff); walk.commit(.2); walk.render(100,1000);
    s.mode=Mode::Walk; auto smooth=color_frame(s,walk.hue(),0,0,false);
    assert(smooth.logical_hue==walk.hue() && smooth.logical_hue!=walk.target());
    s.enabled=false; auto off=color_frame(s,0,0,0,false); assert(off.lit_mask==0); expect_rgb(off.commanded_rgb,0,0,0);
    s.enabled=true; s.mode=Mode::Pixel; s.pixel=63; auto pixel=color_frame(s,0,0,0,false);
    assert(pixel.lit_mask==(UINT64_C(1)<<63)); assert(matrix.show(pixel)); expect_rgb(transmitted_bytes[0],0,0,0);
    s.mode=Mode::Activity; assert(color_frame(s,0,0,0,false).lit_mask==0);
    assert(color_frame(s,0,0,1,false).lit_mask==UINT64_C(0x100000001));
    s.mode=Mode::Experiment; assert(color_frame(s,0,0,0,true).logical_hue==120);

    // Both unchanged and swapped mappings require a separate seven-color pass.
    for(bool original:{false,true}) for(bool swapped:{false,true}) {
        Calibration c; assert(c.start(original,0));
        assert(!c.observe(c.run(),c.step(),TestColor::Red,1)); // cannot confirm before RMT success
        const auto first=c.step(); observe(c,swapped?TestColor::Green:TestColor::Red);
        assert(!c.observe(c.run(),first,TestColor::Green,1000)); // duplicate / stale step
        observe(c,swapped?TestColor::Red:TestColor::Green); observe(c,TestColor::Blue);
        assert(!c.ready() && c.candidate_order()==(original!=swapped));
        for(unsigned i=0;i<7;++i) {
            const auto f=c.frame(true);
            assert(f.brightness==.02 && f.lit_mask==UINT64_MAX);
            assert(f.commanded_rgb.r<=5 && f.commanded_rgb.g<=5 && f.commanded_rgb.b<=5);
            assert(c.expected()==static_cast<TestColor>(i)); observe(c,c.expected());
        }
        assert(c.ready()); c.saved(); assert(!c.active() && !std::strcmp(c.phase_name(),"saved"));
    }
    Calibration c; assert(c.start(true,0)); const auto old_run=c.run(); c.cancel(); assert(c.start(true,0));
    c.presented(old_run,0); assert(!c.presented());
    observe(c,TestColor::Blue); observe(c,TestColor::Green); observe(c,TestColor::Red);
    assert(!c.active() && !std::strcmp(c.phase_name(),"inconclusive"));
    assert(c.start(true,0)); observe(c,TestColor::Red); observe(c,TestColor::Green); observe(c,TestColor::Blue);
    observe(c,TestColor::Other); assert(!c.active());
    assert(c.start(true,0)); c.expire(Calibration::timeout_us); assert(!c.active());
    assert(!std::strcmp(c.phase_name(),"timed_out"));
    assert(c.start(true,0)); c.interrupt(); assert(!c.active());

    // Reopen the actual production settings serializer/store through host NVS.
    assert(write_secret("wifi_v1","private-network-unchanged"));
    assert(write_secret("settings_v1","{\"rgb_order\":true,\"brightness\":0.04}"));
    auto legacy=load_settings(); assert(legacy.rgb_order && legacy.brightness==.04 && legacy.gain_red==1);
    assert(!legacy.channel_order_confirmed); // do not invent a confirmation or change saved order
    legacy.gain_red=.8; legacy.gain_green=1.1; legacy.gain_blue=.65; legacy.channel_order_confirmed=true;
    assert(save_settings(legacy)); auto restored=load_settings();
    assert(restored.rgb_order && restored.channel_order_confirmed && restored.gain_red==.8 && restored.gain_green==1.1 && restored.gain_blue==.65);
    fail_commit=true; legacy.rgb_order=false; assert(!save_settings(legacy)); fail_commit=false;
    assert(load_settings().rgb_order); // failed save does not replace prior calibration
    char wifi[64]; assert(read_secret("wifi_v1",wifi,sizeof(wifi))); assert(!std::strcmp(wifi,"private-network-unchanged"));
    auto before=restored; cJSON* bad=cJSON_Parse("{\"gain_red\":0.9,\"gain_green\":8}");
    assert(!patch_settings(bad,restored) && restored.gain_red==before.gain_red); cJSON_Delete(bad);
    std::puts("Color pipeline, RMT mapping, frame/API snapshot, calibration state machine, and NVS persistence passed");
}

#include "lighting/core.hpp"
#include "entropy/deterministic.hpp"
#include <cassert>
#include <limits>
#include <cstdio>
using namespace oml;
int main() {
    Settings s; assert(valid(s)); s.brightness=1; assert(!valid(s));
    s=Settings{}; s.sensitivity=std::numeric_limits<double>::quiet_NaN(); assert(!valid(s));
    assert(net_bits(0)==-32 && net_bits(0xffffffff)==32 && net_bits(0xaaaaaaaa)==0);
    const RGB red=hsv(0,1,.125), green=hsv(120,1,.125), blue=hsv(240,1,.125);
    assert(red.r==32 && red.g==0 && red.b==0); assert(green.g==32 && blue.b==32);
    for(int h=-720;h<720;++h) { auto p=hsv(h,.7,1); assert(p.r<=32 && p.g<=32 && p.b<=32); }
    Walk a,b; a.reset(359); b.reset(359);
    a.sample(0xffffffff); b.sample(0xffffffff); a.commit(.2); b.commit(.2);
    assert(std::abs(a.target()-5.4)<1e-9);
    for(int i=0;i<30;++i) a.render(1000.0/30,1500);
    for(int i=0;i<60;++i) b.render(1000.0/60,1500);
    assert(std::abs(a.hue()-b.hue())<1e-9);
    // Same raw stream, differing animation rates: identical targets and sample totals.
    DeterministicTestSource rng(42), duplicate(42); Walk slow, fast;
    EntropySource& test_source=rng; assert(!test_source.experimental());
    Health health; int64_t sum=0;
    for(int i=0;i<100000;++i) {
        uint32_t word; assert(test_source.read(word)); assert(word==duplicate.next()); sum+=net_bits(word);
        assert(health.observe(word)); slow.sample(word); fast.sample(word);
        fast.render(1,1500); if(i%10==0) slow.render(10,1500);
        if(i%20==19) {slow.commit(.2);fast.commit(.2);}
    }
    assert(slow.target()==fast.target()); assert(health.words==100000);
    assert(sum==2*static_cast<int64_t>(health.ones)-32*100000);
    Health stuck; assert(stuck.observe(0)); assert(!stuck.observe(0)); assert(!stuck.observe(0xaaaaaaaa));
    Health biased; for(int i=0;i<32;++i) biased.observe(0xeeeeeeee); assert(biased.failed());
    std::puts("Core invariants passed: frame independence, power ceiling, hue wrap, raw accounting, health alarms");
}

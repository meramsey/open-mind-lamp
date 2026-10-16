#pragma once
#include "lighting/core.hpp"
#include <cstddef>
namespace oml {
enum class TraceKind : uint8_t { Reset, Sample, Commit };
struct TraceEvent {
    uint64_t seq=0, sample_id=0;
    int64_t at_us=0;
    uint64_t window_id=0;
    double before=0, after=0;
    uint32_t word=0;
    int32_t net=0;
    uint32_t count=0;
    TraceKind kind=TraceKind::Reset;
    bool healthy=true, applied=false;
};
static_assert(sizeof(TraceEvent)<=64,"Keep the diagnostic event bounded");
// Always-on, fixed RAM ring: no flash writes, capture mode, sample injection,
// experiment allocation, or extra hardware RNG reads per sample/poll. A boot
// nonce is generated once and is never counted as a measurement.
struct RngDiagnostics {
    static constexpr size_t capacity=128;
    TraceEvent events[capacity]{};
    uint64_t sequence=0, reset_sequence=1;
    uint64_t rejected_reads=0;
    uint32_t boot_id=0;
    int64_t sampled_at_us=0, window_started_us=0;
    bool last_applied=false;
    void add(TraceEvent event) { event.seq=++sequence; events[(sequence-1)%capacity]=event; }
    void reset(const Walk& w,uint64_t samples,int64_t now) {
        window_started_us=now;
        TraceEvent e; e.kind=TraceKind::Reset; e.at_us=now; e.sample_id=samples;
        e.after=w.target(); add(e); reset_sequence=sequence;
    }
    void sample(const Walk& w,uint64_t samples,uint32_t word,bool healthy,int64_t now) {
        sampled_at_us=now; last_applied=healthy;
        TraceEvent e; e.kind=TraceKind::Sample; e.at_us=now; e.sample_id=samples;
        e.window_id=w.window_id(); e.word=word; e.net=net_bits(word);
        e.healthy=healthy; e.applied=healthy; add(e);
    }
    void commit(const WalkCommit& c,uint64_t samples,int64_t now) {
        TraceEvent e; e.kind=TraceKind::Commit; e.at_us=now; e.sample_id=samples;
        e.window_id=c.window_id; e.net=c.net; e.count=c.samples; e.before=c.before; e.after=c.after;
        add(e); window_started_us=now;
    }
    uint64_t oldest() const {
        const auto retained=sequence>=capacity ? sequence-capacity+1 : 1;
        return retained>reset_sequence ? retained : reset_sequence;
    }
};
}

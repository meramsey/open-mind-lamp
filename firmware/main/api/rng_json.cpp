#include "rng_json.hpp"
#include "color_json.hpp"
#include "storage/settings.hpp"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
namespace oml {
cJSON* rng_json(const RngDiagnostics& d,const Walk& w,const Settings& s,const Health& h,
                uint32_t word,bool ready,int64_t now) {
    auto* j=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"source",hardware_source_name);
    cJSON_AddBoolToObject(j,"ready",ready);
    cJSON_AddBoolToObject(j,"health_failed",h.failed());
    cJSON_AddNumberToObject(j,"successful_samples",static_cast<double>(h.words));
    cJSON_AddNumberToObject(j,"rejected_reads",static_cast<double>(d.rejected_reads));
    cJSON_AddNumberToObject(j,"sample_hz",s.sample_hz);
    cJSON_AddNumberToObject(j,"accumulation_ms",s.accumulation_ms);
    cJSON_AddNumberToObject(j,"frame_hz",s.frame_hz);
    if (h.words) {
        char hex[11]; std::snprintf(hex,sizeof(hex),"0x%08" PRIx32,word);
        cJSON_AddNumberToObject(j,"latest_word",word); cJSON_AddStringToObject(j,"latest_word_hex",hex);
        cJSON_AddNumberToObject(j,"ones",__builtin_popcount(word));
        cJSON_AddNumberToObject(j,"zeros",32-__builtin_popcount(word));
        cJSON_AddNumberToObject(j,"net",net_bits(word));
        cJSON_AddBoolToObject(j,"latest_applied",d.last_applied);
        cJSON_AddNumberToObject(j,"sampled_at_us",static_cast<double>(d.sampled_at_us));
        cJSON_AddNumberToObject(j,"sample_age_ms",std::max<int64_t>(0,now-d.sampled_at_us)/1000.0);
    } else cJSON_AddNullToObject(j,"latest_word");
    cJSON_AddNumberToObject(j,"window_id",static_cast<double>(w.window_id()));
    cJSON_AddNumberToObject(j,"window_samples",w.pending_samples());
    cJSON_AddNumberToObject(j,"window_net",w.pending_net());
    cJSON_AddNumberToObject(j,"pending_movement_deg",w.pending_net()*s.sensitivity);
    cJSON_AddNumberToObject(j,"window_started_us",static_cast<double>(d.window_started_us));
    cJSON_AddNumberToObject(j,"target_hue",w.target());
    cJSON_AddNumberToObject(j,"smoothed_hue",w.hue());
    const auto& c=w.last_commit();
    auto* last=cJSON_AddObjectToObject(j,"last_commit");
    cJSON_AddBoolToObject(last,"valid",w.window_id()>0);
    cJSON_AddNumberToObject(last,"window_id",static_cast<double>(c.window_id));
    cJSON_AddNumberToObject(last,"samples",c.samples); cJSON_AddNumberToObject(last,"net",c.net);
    cJSON_AddNumberToObject(last,"movement_deg",c.movement_deg);
    cJSON_AddNumberToObject(last,"target_before",c.before); cJSON_AddNumberToObject(last,"target_after",c.after);
    return j;
}
cJSON* trace_metadata_json(const TraceSnapshot& s) {
    auto* j=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"type","rng_pipeline_trace"); cJSON_AddNumberToObject(j,"schema",1);
    cJSON_AddStringToObject(j,"source",hardware_source_name); cJSON_AddBoolToObject(j,"experimental_record",false);
    cJSON_AddNumberToObject(j,"boot_id",s.rng.boot_id); cJSON_AddNumberToObject(j,"revision",s.revision);
    cJSON_AddNumberToObject(j,"captured_at_us",static_cast<double>(s.now_us));
    cJSON_AddNumberToObject(j,"capacity",RngDiagnostics::capacity);
    cJSON_AddNumberToObject(j,"oldest_seq",static_cast<double>(s.rng.oldest()));
    cJSON_AddNumberToObject(j,"latest_seq",static_cast<double>(s.rng.sequence));
    cJSON_AddBoolToObject(j,"calibration_active",s.calibration_active);
    cJSON_AddBoolToObject(j,"experiment_active",s.experiment_active);
    cJSON_AddItemToObject(j,"settings",settings_json(s.settings));
    cJSON_AddItemToObject(j,"rng",rng_json(s.rng,s.walk,s.settings,s.health,s.latest_word,s.ready,s.now_us));
    cJSON_AddItemToObject(j,"color",color_json(s.color,s.now_us));
    return j;
}
cJSON* trace_event_json(const TraceEvent& e) {
    auto* item=cJSON_CreateObject();
    cJSON_AddStringToObject(item,"kind",e.kind==TraceKind::Sample ? "sample" : e.kind==TraceKind::Commit ? "commit" : "reset");
    cJSON_AddNumberToObject(item,"seq",static_cast<double>(e.seq));
    cJSON_AddNumberToObject(item,"at_us",static_cast<double>(e.at_us));
    cJSON_AddNumberToObject(item,"sample_id",static_cast<double>(e.sample_id));
    cJSON_AddNumberToObject(item,"window_id",static_cast<double>(e.window_id));
    if (e.kind==TraceKind::Sample) {
        cJSON_AddNumberToObject(item,"word",e.word); cJSON_AddNumberToObject(item,"net",e.net);
        cJSON_AddBoolToObject(item,"healthy",e.healthy); cJSON_AddBoolToObject(item,"applied",e.applied);
    } else {
        cJSON_AddNumberToObject(item,"target_after",e.after);
        if (e.kind==TraceKind::Commit) {
            cJSON_AddNumberToObject(item,"samples",e.count); cJSON_AddNumberToObject(item,"net",e.net);
            cJSON_AddNumberToObject(item,"target_before",e.before);
        }
    }
    return item;
}
cJSON* trace_json(const TraceSnapshot& s,uint64_t after) {
    auto* j=trace_metadata_json(s);
    auto* list=cJSON_AddArrayToObject(j,"events");
    for (uint64_t seq=s.rng.oldest();seq<=s.rng.sequence;++seq) {
        if (seq<=after) continue;
        const auto& e=s.rng.events[(seq-1)%RngDiagnostics::capacity];
        cJSON_AddItemToArray(list,trace_event_json(e));
    }
    return j;
}
}

#pragma once
#include "entropy/diagnostics.hpp"
#include "entropy/interface.hpp"
#include "lighting/color.hpp"
#include "cJSON.h"
namespace oml {
cJSON* rng_json(const RngDiagnostics&,const Walk&,const Settings&,const Health&,
                uint32_t latest_word,bool ready,int64_t now_us);
// Heap-allocated only for explicit trace GETs, copied under the application lock.
// Serialization/sending runs outside the sampling mutex.
struct TraceSnapshot {
    RngDiagnostics rng;
    Walk walk;
    Settings settings;
    Health health;
    RenderedColor color;
    uint32_t latest_word=0, revision=0;
    bool ready=false, calibration_active=false, experiment_active=false;
    int64_t now_us=0;
};
cJSON* trace_json(const TraceSnapshot&,uint64_t after);
cJSON* trace_metadata_json(const TraceSnapshot&);
cJSON* trace_event_json(const TraceEvent&);
}

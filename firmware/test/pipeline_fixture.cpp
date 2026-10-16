#include "pipeline_sequence.hpp"
#include "api/rng_json.hpp"
#include "lighting/matrix.hpp"
#include <cassert>
#include <cstdio>
using namespace oml;
int main() {
    diagnostic_test::KnownSequence source;
    TraceSnapshot s; s.settings.starting_hue=359; s.settings.sensitivity=.5;
    s.settings.saturation=1; s.settings.brightness=.04; s.settings.smoothing_ms=200;
    s.settings.accumulation_ms=100;
    s.walk.reset(359); s.rng.boot_id=12345; s.rng.reset(s.walk,0,0); s.ready=true;
    Matrix matrix; matrix.init(); uint64_t cursor=0;
    auto* document=cJSON_CreateObject(); cJSON_AddStringToObject(document,"type","open_mind_lamp_live_validation");
    cJSON_AddNumberToObject(document,"schema",1); cJSON_AddBoolToObject(document,"experimental_record",false);
    auto* snapshots=cJSON_AddArrayToObject(document,"snapshots");
    for(unsigned window=0;window<4;++window) {
        for(unsigned i=0;i<2;++i) {
            assert(source.read(s.latest_word)); assert(s.health.observe(s.latest_word)); s.walk.sample(s.latest_word);
            s.now_us+=50000; s.rng.sample(s.walk,s.health.words,s.latest_word,true,s.now_us);
        }
        s.rng.commit(s.walk.commit(s.settings.sensitivity),s.health.words,++s.now_us);
        const auto step=s.walk.render(100,s.settings.smoothing_ms);
        const auto frame=color_frame(s.settings,s.walk.hue(),s.now_us/1e6,s.latest_word,false);
        assert(matrix.show(frame)); s.color.transmitted(frame,s.now_us+200,true,&step); s.now_us+=300;
        auto* snapshot=trace_json(s,cursor); cursor=s.rng.sequence;
        // Never label deterministic host fixtures as genuine hardware acquisition.
        cJSON_ReplaceItemInObjectCaseSensitive(snapshot,"source",cJSON_CreateString(source.name()));
        cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(snapshot,"rng"),"source",cJSON_CreateString(source.name()));
        cJSON_AddItemToArray(snapshots,snapshot);
    }
    char* text=cJSON_PrintUnformatted(document); std::puts(text); cJSON_free(text); cJSON_Delete(document);
}

#include "lighting/color.hpp"
#include "api/color_json.hpp"
#include <cstdio>
#include <initializer_list>
using namespace oml;
int main() {
    cJSON* frames=cJSON_CreateArray();
    Settings s; s.mode=Mode::Ambient; s.brightness=.02; s.saturation=1;
    const double hues[]={0,120,240,0,60,180,300};
    RenderedColor state;
    for(unsigned i=0;i<7;++i) {
        s.starting_hue=hues[i]; s.saturation=i==3?0:1;
        state.transmitted(color_frame(s,73,0,0,false),10000+i*1000,true);
        cJSON_AddItemToArray(frames,color_json(state,20000+i*1000));
    }
    s.mode=Mode::Breathing; s.saturation=.7;
    for(double time:{1.25,3.75}) {
        state.transmitted(color_frame(s,32,time,0,false),30000,true);
        cJSON_AddItemToArray(frames,color_json(state,40000));
    }
    s.enabled=false; state.transmitted(color_frame(s,32,0,0,false),50000,true);
    cJSON_AddItemToArray(frames,color_json(state,60000));
    state.transmitted(state.frame,70000,false); cJSON_AddItemToArray(frames,color_json(state,80000));
    char* text=cJSON_PrintUnformatted(frames); std::puts(text); cJSON_free(text); cJSON_Delete(frames);
}

#include "chams_appearance.h"
#include <cassert>
#include <limits>
#include <cstring>
namespace {
FILE* input(const char* text){FILE* file=std::tmpfile();assert(file);assert(std::fwrite(text,1,std::strlen(text),file)==std::strlen(text));std::rewind(file);return file;}
void finite(const std::array<mango_chams::Color,3>& colors){
    for(auto c:colors)for(float v:{c.r,c.g,c.b,c.a})assert(std::isfinite(v)&&v>=0&&v<=1);
}
}
int main(){
    using namespace mango_chams;
    Appearance a;float alpha=.28f;auto classic=palette(a,alpha,0);
    assert(classic[0].r==.12f&&classic[0].g==.83f&&classic[1].r==1&&classic[2].r==.65f);
    assert(same(classic,palette(a,alpha,100)));
    for(int preset=0;preset<custom_preset;++preset){
        apply_preset(a,alpha,preset);assert(a.preset==preset);finite(palette(a,alpha,0));
        char record[256];assert(save_appearance(record,sizeof(record),a));
        Appearance roundtrip;FILE* file=input(record);assert(load_appearance(file,roundtrip));std::fclose(file);
        assert(roundtrip.preset==a.preset&&roundtrip.style==a.style&&same(palette(a,alpha,0),palette(roundtrip,alpha,0)));
    }
    apply_preset(a,alpha,1);assert(a.rgb[0][0]==.35f&&a.rgb[1][2]==1);
    apply_preset(a,alpha,3);assert(a.style==Soft&&palette(a,alpha,0)[0].a<.21f);
    apply_preset(a,alpha,4);assert(a.style==Flat&&palette(a,alpha,0)[0].a==1);
    apply_preset(a,alpha,5);auto low=palette(a,alpha,.75),high=palette(a,alpha,.25);
    assert(low[0].a<high[0].a&&low[0].r==high[0].r&&low[0].a>=alpha*.599f);
    apply_preset(a,alpha,6);auto start=palette(a,alpha,0),later=palette(a,alpha,1);
    assert(start[0].r!=later[0].r||start[0].g!=later[0].g);assert(start[2].r==later[2].r&&start[2].g==later[2].g);
    const auto prior=a;apply_preset(a,alpha,custom_preset);assert(a.preset==custom_preset&&a.style==prior.style);
    for(const char* old:{"","\n","MANGO_ESP_V1 1 1","CHAMS_STYLE_V1 3 1 1 0.5"}){
        Appearance loaded;FILE* file=input(old);assert(!load_appearance(file,loaded));std::fclose(file);
        assert(loaded.preset==0&&loaded.style==Flat);
    }
    FILE* bad=input("CHAMS_STYLE_V1 999 99 nan -2 9 inf 0 0 0 0 0 0\n");
    assert(load_appearance(bad,a));std::fclose(bad);assert(a.preset==custom_preset&&a.style==Flat&&a.speed==1);
    assert(a.rgb[0][0]==0&&a.rgb[0][1]==1&&a.rgb[0][2]==1);
    char short_record[5];assert(!save_appearance(short_record,sizeof(short_record),a));
    for(int style=Flat;style<=Rainbow;++style){
        a.style=style;a.speed=3;
        for(double time:{-1.0,0.0,.125,1e9,std::numeric_limits<double>::max(),std::numeric_limits<double>::infinity()})finite(palette(a,alpha,time));
    }
    std::puts("PASS: seven chams presets, animation bounds, palette editing, tagged persistence, old/truncated/invalid config defaults");
}

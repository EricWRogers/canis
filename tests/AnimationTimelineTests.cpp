#include <Canis/AnimationTimeline.hpp>
#include <iostream>
#include <stdexcept>
using namespace Canis;
using namespace Canis::AnimationTimeline;
static void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
static void Near(float a,float b,const char* why) { Check(std::isfinite(a)&&std::abs(a-b)<.0001f,why); }
int main()
{
    try
    {
        Check(Frame(.5f)==30 && Frame(-1)==0 && Frame(std::numeric_limits<float>::infinity())==0,"frame conversion bounds");
        Check(Frame(std::numeric_limits<float>::max())==std::numeric_limits<int>::max(),"frame overflow clamp");
        Near(Time(30),.5f,"frame time"); Near(Time(-1),0,"negative frame");
        Near(Normalize(2.5f,2,true),.5f,"loop"); Near(Normalize(-.5f,2,true),1.5f,"reverse loop");
        Near(Normalize(4,2,false),2,"clamp"); Near(Normalize(1,0,true),0,"zero length");
        Near(Normalize(std::numeric_limits<float>::infinity(),2,true),0,"invalid time");
        for(float fps:{30.f,60.f,120.f,240.f})
        {
            float time=0; bool playing=true;
            for(int i=0;i<int(fps);++i)
            {
                Advance(time,playing,1/fps,2,1,true);
                (void)Frame(time);
            }
            Near(time,1,"timeline display must not alter playback speed");
        }
        float playback=0; bool playing=true;
        Advance(playback,playing,1,2,2,false); Near(playback,2,"playback speed");
        Check(!playing,"one shot stops at end");
        Advance(playback,playing,1,2,1,false); Near(playback,2,"paused holds endpoint");
        playback=1.5f; playing=true; Advance(playback,playing,1,2,1,true); Near(playback,.5f,"loop continues");
        Advance(playback,playing,std::numeric_limits<float>::infinity(),2,1,true); Near(playback,.5f,"invalid delta ignored");
        Near(TimeAtFraction(.5f,2,4,false),4,"zoomed viewport midpoint");
        Near(TimeAtFraction(-1,2,4,false),2,"viewport left clamp");
        Near(TimeAtFraction(2,2,4,false),6,"viewport right clamp");
        Near(TimeAtFraction(.123f,0,1,true),7.f/60,"snap to timeline frame");
        Near(TimeAtFraction(.123f,0,1,false),.123f,"subframe scrub without snap");
        AnimationClipAsset clip; clip.length=2; clip.tracks.resize(2);
        UpsertKey(clip.tracks[0],1,AnimationValue::Float(1));
        UpsertKey(clip.tracks[0],0,AnimationValue::Float(0));
        UpsertKey(clip.tracks[0],1,AnimationValue::Float(3));
        Check(clip.tracks[0].keys.size()==2 && clip.tracks[0].keys.back().value.AsFloat()==3,"capture updates existing key");
        UpsertKey(clip.tracks[0],1.00001f,AnimationValue::Float(4));
        Check(clip.tracks[0].keys.size()==2,"near-duplicate keys merged");
        UpsertKey(clip.tracks[0],std::numeric_limits<float>::quiet_NaN(),{});
        Check(clip.tracks[0].keys.size()==2,"invalid key ignored");
        UpsertKey(clip.tracks[1],.5f,AnimationValue::Float(7));
        AnimationEvent event; event.time=1.5f; clip.events.push_back(event);
        Near(AdjacentKey(clip,0,true),.5f,"next key across tracks");
        Near(AdjacentKey(clip,0,true,0),1,"next key selected track");
        Near(AdjacentKey(clip,1,false),.5f,"previous key excludes current key");
        Near(AdjacentKey(clip,1,true),1.5f,"event navigation");
        Near(AdjacentKey(clip,1.9f,true),2,"last fallback");
        Near(AdjacentKey(clip,0,false),0,"first fallback");
    }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
    std::cout<<"Animation timeline playback, seeking, key navigation and editing passed\n";
}

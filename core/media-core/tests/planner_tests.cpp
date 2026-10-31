#include "telegram_media/planner.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace telegram_media;
namespace {
int failures=0;
void expect(bool ok,const std::string& message){ if(!ok){ std::cerr<<"FAIL: "<<message<<'\n'; ++failures; } }
MediaInfo sample(){ MediaInfo m; m.container="mp4"; m.streams.push_back(StreamInfo{MediaKind::Video,"h264",1920,1080,{},{},false}); m.streams.push_back(StreamInfo{MediaKind::Audio,"aac",{},{},48000,2,false}); return m; }
}
int main(){
 { auto s=sample(); ConversionRequest r; r.target_container="mp4"; auto p=create_plan(s,r); expect(p.ok,"same container creates plan"); expect(p.plan.operation==Operation::Remux,"same container prefers remux"); expect(validate_plan(s,r,p.plan).empty(),"valid plan passes policy"); }
 { auto s=sample(); ConversionRequest r; r.target_container="mkv"; r.target_video_codec="hevc"; r.target_audio_codec="aac"; auto p=create_plan(s,r); expect(p.ok,"transcode plan accepted"); expect(p.plan.operation==Operation::Transcode,"codec/container change transcodes"); }
 { auto s=sample(); ConversionRequest r; r.target_container="mkv"; r.allow_transcode=false; expect(!create_plan(s,r).ok,"disabled transcode rejected"); }
 { auto s=sample(); ConversionRequest r; r.target_container="mp4"; r.overwrite_source=true; expect(!create_plan(s,r).ok,"source overwrite rejected"); }
 { MediaInfo s; ConversionRequest r; r.target_container="mp4"; expect(!create_plan(s,r).ok,"empty source metadata rejected"); }
 { auto s=sample(); ConversionRequest r; r.target_container="mkv"; r.target_video_codec="hevc"; r.allow_transcode=false; ConversionPlan bad; bad.operation=Operation::Transcode; bad.target_container="mkv"; expect(!validate_plan(s,r,bad).empty(),"policy rejects forbidden transcode"); }
 if(failures) return EXIT_FAILURE; std::cout<<"All Shared Media Core tests passed.\n"; return EXIT_SUCCESS;
}

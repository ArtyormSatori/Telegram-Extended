#include "telegram_media/planner.hpp"
#include <algorithm>
#include <cctype>
#include <utility>
namespace telegram_media { namespace {
std::string normalize(std::string value) {
 std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
 if (!value.empty() && value.front()=='.') value.erase(value.begin());
 return value;
}
bool has_kind(const MediaInfo& source, MediaKind kind) {
 return std::any_of(source.streams.begin(), source.streams.end(), [kind](const StreamInfo& s){ return s.kind==kind; });
}
bool container_matches(const std::string& reported, const std::string& requested) {
 std::size_t start=0;
 while(start<=reported.size()) {
  const auto end=reported.find(',',start);
  if(normalize(reported.substr(start,end==std::string::npos?std::string::npos:end-start))==requested) return true;
  if(end==std::string::npos) break;
  start=end+1U;
 }
 return false;
}
} // namespace
PlanResult create_plan(const MediaInfo& source, const ConversionRequest& request) {
 PlanResult result; const auto target=normalize(request.target_container);
 if (target.empty()) { result.errors.emplace_back("Target container is required."); return result; }
 if (request.overwrite_source) { result.errors.emplace_back("Overwriting the source file is prohibited."); return result; }
 if (source.streams.empty()) { result.errors.emplace_back("Source has no inspected streams; probe it before planning."); return result; }
 ConversionPlan plan; plan.target_container=target; plan.video_codec=request.target_video_codec; plan.audio_codec=request.target_audio_codec;
 const bool codec_change=request.target_video_codec.has_value() || request.target_audio_codec.has_value();
 const bool same_container=container_matches(source.container,target);
 if (same_container && !codec_change) {
  plan.operation=Operation::Remux;
  plan.rationale.emplace_back("Container unchanged and no codec change requested; stream copy may avoid quality loss.");
 } else {
  if (!request.allow_transcode) { result.errors.emplace_back("Requested output requires transcode, but transcoding is disabled."); return result; }
  plan.operation=Operation::Transcode;
  plan.rationale.emplace_back("Container or codec change requested; backend compatibility must be verified before execution.");
 }
 if (has_kind(source,MediaKind::Video) && !plan.video_codec && plan.operation==Operation::Transcode) result.errors.emplace_back("Video codec required for video transcode.");
 if (has_kind(source,MediaKind::Audio) && !plan.audio_codec && plan.operation==Operation::Transcode && !has_kind(source,MediaKind::Video)) result.errors.emplace_back("Audio codec required for audio-only transcode.");
 if (!result.errors.empty()) return result; result.ok=true; result.plan=std::move(plan); return result;
}
std::vector<std::string> validate_plan(const MediaInfo& source,const ConversionRequest& request,const ConversionPlan& plan) {
 std::vector<std::string> errors;
 if (request.overwrite_source) errors.emplace_back("Source overwrite is not allowed.");
 if (normalize(request.target_container)!=normalize(plan.target_container)) errors.emplace_back("Plan target does not match user request.");
 if (plan.operation==Operation::Transcode && !request.allow_transcode) errors.emplace_back("Transcoding is disabled by user.");
 if (plan.operation==Operation::Remux && (request.target_video_codec || request.target_audio_codec)) errors.emplace_back("Remux cannot satisfy explicit codec change.");
 if (source.streams.empty()) errors.emplace_back("Source metadata is empty.");
 return errors;
}
} // namespace telegram_media

#include "telegram_media/ffprobe_adapter.hpp"
#include <charconv>
#include <map>
#include <optional>
#include <string_view>
namespace telegram_media { namespace {
std::string trim(std::string value) {
 const auto first=value.find_first_not_of(" \t\r\n");
 if(first==std::string::npos) return {};
 const auto last=value.find_last_not_of(" \t\r\n");
 return value.substr(first,last-first+1);
}
std::string unquote(std::string value) {
 value=trim(std::move(value));
 if(value.size()>=2 && value.front()=='"' && value.back()=='"') return value.substr(1,value.size()-2);
 return value;
}
std::optional<std::uint64_t> number(const std::map<std::string,std::string>& values,const std::string& key) {
 const auto it=values.find(key); if(it==values.end()) return std::nullopt;
 std::uint64_t parsed{}; const auto* begin=it->second.data(); const auto* end=begin+it->second.size();
 const auto result=std::from_chars(begin,end,parsed);
 if(result.ec!=std::errc{} || result.ptr!=end) return std::nullopt;
 return parsed;
}
std::string get(const std::map<std::string,std::string>& values,const std::string& key) {
 const auto it=values.find(key); return it==values.end()?std::string{}:it->second;
}
bool is_hdr(const std::map<std::string,std::string>& values) {
 const auto transfer=get(values,"color_transfer"); const auto primaries=get(values,"color_primaries");
 return transfer=="smpte2084" || transfer=="arib-std-b67" || primaries=="bt2020";
}
} // namespace
FFprobeAdapter::FFprobeAdapter(std::string executable):executable_(std::move(executable)){}
std::vector<std::string> FFprobeAdapter::build_arguments(const std::string& local_path) const {
 return {executable_,"-v","error","-show_entries",
  "format=format_name,duration,size:stream=codec_type,codec_name,width,height,sample_rate,channels,color_transfer,color_primaries",
  "-of","flat","-i",local_path};
}
ProbeResult FFprobeAdapter::probe(const std::string& local_path,const IProcessRunner& runner,std::chrono::milliseconds timeout) const {
 ProbeResult result;
 if(local_path.empty()){result.errors.emplace_back("A local media path is required.");return result;}
 const auto process=runner.run(build_arguments(local_path),timeout);
 if(process.timed_out){result.errors.emplace_back("FFprobe timed out.");return result;}
 if(process.exit_code!=0){result.errors.emplace_back(process.standard_error.empty()?"FFprobe failed.":process.standard_error);return result;}
 return parse_flat_output(process.standard_output);
}
ProbeResult FFprobeAdapter::parse_flat_output(const std::string& output) {
 ProbeResult result; std::map<std::string,std::string> format;
 std::map<std::size_t,std::map<std::string,std::string>> streams;
 std::size_t offset=0;
 while(offset<output.size()){
  const auto end=output.find('\n',offset);
  const auto line=trim(output.substr(offset,end==std::string::npos?std::string::npos:end-offset));
  offset=end==std::string::npos?output.size():end+1;
  if(line.empty()) continue;
  const auto equals=line.find('='); if(equals==std::string::npos) continue;
  const auto key=line.substr(0,equals); const auto value=unquote(line.substr(equals+1));
  if(key.rfind("format.",0)==0){format[key.substr(7)]=value;continue;}
  constexpr std::string_view prefix="streams.stream.";
  if(key.rfind(prefix,0)!=0) continue;
  const auto index_start=prefix.size(); const auto index_end=key.find('.',index_start);
  if(index_end==std::string::npos) continue;
  std::size_t index{}; const auto index_text=std::string_view(key).substr(index_start,index_end-index_start);
  const auto parsed=std::from_chars(index_text.data(),index_text.data()+index_text.size(),index);
  if(parsed.ec!=std::errc{} || parsed.ptr!=index_text.data()+index_text.size()) continue;
  streams[index][key.substr(index_end+1)]=value;
 }
 result.media.container=get(format,"format_name");
 const auto duration=get(format,"duration");
 if(!duration.empty()) try { const auto seconds=std::stod(duration); if(seconds>=0.0) result.media.duration_ms=static_cast<std::uint64_t>(seconds*1000.0); } catch(...) {}
 result.media.size_bytes=number(format,"size");
 for(const auto& [index,fields]:streams){
  (void)index; StreamInfo stream; const auto type=get(fields,"codec_type");
  if(type=="video") stream.kind=MediaKind::Video;
  else if(type=="audio") stream.kind=MediaKind::Audio;
  else if(type=="subtitle") stream.kind=MediaKind::Subtitle;
  else continue;
  stream.codec=get(fields,"codec_name");
  if(const auto v=number(fields,"width")) stream.width=static_cast<std::uint32_t>(*v);
  if(const auto v=number(fields,"height")) stream.height=static_cast<std::uint32_t>(*v);
  if(const auto v=number(fields,"sample_rate")) stream.sample_rate=static_cast<std::uint32_t>(*v);
  if(const auto v=number(fields,"channels")) stream.channels=static_cast<std::uint32_t>(*v);
  stream.hdr=is_hdr(fields); result.media.streams.push_back(std::move(stream));
 }
 if(result.media.container.empty()) result.errors.emplace_back("FFprobe output did not contain a format name.");
 if(result.media.streams.empty()) result.errors.emplace_back("FFprobe output did not contain supported audio/video/subtitle streams.");
 result.ok=result.errors.empty(); return result;
}
} // namespace telegram_media

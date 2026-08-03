#include "platform/single_instance_name.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
namespace squiflow::platform {
namespace {
bool valid_id(const std::string& id){if(id.empty()||id.size()>48)return false;return std::all_of(id.begin(),id.end(),[](char c){const unsigned char u=static_cast<unsigned char>(c);return std::isalnum(u)!=0||c=='.'||c=='-'||c=='_';});}
std::uint64_t hash(const std::string& value){std::uint64_t h=1469598103934665603ULL;for(char c:value){h^=static_cast<unsigned char>(c);h*=1099511628211ULL;}return h;}
}
SingleInstanceNames make_single_instance_names(const std::string& application_id,const std::string& data_directory){SingleInstanceNames result;if(!valid_id(application_id)){result.error="The application identifier is invalid.";return result;}std::filesystem::path path(data_directory);if(data_directory.empty()||!path.is_absolute()){result.error="The data directory must be absolute.";return result;}std::string normalized=path.lexically_normal().generic_string();if(normalized.find("..")!=std::string::npos||normalized.size()>200){result.error="The data directory cannot identify an instance.";return result;}std::transform(normalized.begin(),normalized.end(),normalized.begin(),[](char c){return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));});std::ostringstream suffix;suffix<<std::hex<<std::setw(16)<<std::setfill('0')<<hash(normalized);const std::string base="Global\\"+application_id+"."+suffix.str();result.mutex_name=base+".Mutex";result.activation_name=base+".Activate";result.ok=result.mutex_name.size()<240&&result.activation_name.size()<240;if(!result.ok)result.error="The instance name is too long.";return result;}
}  // namespace squiflow::platform

#pragma once
#include <string>
namespace squiflow::platform {
struct SingleInstanceNames { bool ok{false}; std::string mutex_name{}; std::string activation_name{}; std::string error{}; };
SingleInstanceNames make_single_instance_names(const std::string& application_id,const std::string& data_directory);
}  // namespace squiflow::platform

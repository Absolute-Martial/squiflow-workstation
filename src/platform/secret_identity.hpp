#pragma once
#include <string>
#include <string_view>
namespace squiflow::platform {
bool is_valid_secret_key(std::string_view key)noexcept;
std::string secret_file_name(std::string_view key);
} // namespace squiflow::platform

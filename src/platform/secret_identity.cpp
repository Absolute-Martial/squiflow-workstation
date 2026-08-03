#include "platform/secret_identity.hpp"
#include "platform/secrets.hpp"
#include <stdexcept>
namespace squiflow::platform {
bool is_valid_secret_key(std::string_view key)noexcept{if(key.empty()||key.size()>kMaxSecretKeyLength||key.front()=='.'||key.back()=='.')return false;char previous='\0';for(char c:key){const bool valid=(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';if(!valid||(c=='.'&&previous=='.'))return false;previous=c;}return true;}
std::string secret_file_name(std::string_view key){if(!is_valid_secret_key(key))throw std::invalid_argument("invalid secret key");return std::string(key)+".dpapi";}
} // namespace squiflow::platform

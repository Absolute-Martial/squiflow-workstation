#include "platform/secrets.hpp"
#include "platform/secret_envelope.hpp"
#include "platform/secret_identity.hpp"
#include <windows.h>
#include <dpapi.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <memory>
#include <span>
#include <cstddef>
#include <string_view>
namespace squiflow::platform { namespace {
std::wstring wide(const std::string& s){return {s.begin(),s.end()};}
class DpapiStore final:public SecretStore{public:explicit DpapiStore(std::string root):root_(std::move(root)){}
SecretWriteResult store(std::string_view key,std::span<const std::byte> value)override{if(!is_valid_secret_key(key))return {false,SecretFault::InvalidKey,"Invalid secret key."};if(value.empty())re[...]
SecretReadResult load(std::string_view key)override{if(!is_valid_secret_key(key))return {false,SecretFault::InvalidKey,{},"Invalid secret key."};std::ifstream input(root_+"/"+secret_file_name(key)[...]
SecretDeleteResult erase(std::string_view key)override{if(!is_valid_secret_key(key))return {false,false,SecretFault::InvalidKey,"Invalid secret key."};const auto path=wide(root_+"/"+secret_file_na[...]
private:std::string root_;};}
std::unique_ptr<SecretStore> make_secret_store(const std::string& directory){return std::make_unique<DpapiStore>(directory);} }

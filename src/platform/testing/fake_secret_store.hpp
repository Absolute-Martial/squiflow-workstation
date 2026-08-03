#pragma once
#include <map>
#include <string>
#include <vector>
#include "platform/secret_identity.hpp"
#include "platform/secrets.hpp"
namespace squiflow::platform::testing {
class FakeSecretStore final:public SecretStore{public:SecretFault next_fault{SecretFault::None};SecretWriteResult store(std::string_view key,std::span<const std::byte> value)override{if(!is_valid_secret_key(key))return {false,SecretFault::InvalidKey,"Invalid secret key."};if(value.empty())return {false,SecretFault::EmptyValue,"Secret value is empty."};if(value.size()>kMaxSecretValueLength)return {false,SecretFault::ValueTooLarge,"Secret value is too large."};if(next_fault!=SecretFault::None){const auto f=take_fault();return {false,f,"Injected secret-store failure."};}values_[std::string(key)]={value.begin(),value.end()};return {true,SecretFault::None,{}};}SecretReadResult load(std::string_view key)override{if(!is_valid_secret_key(key))return {false,SecretFault::InvalidKey,{},"Invalid secret key."};if(next_fault!=SecretFault::None){const auto f=take_fault();return {false,f,{},"Injected secret-store failure."};}const auto it=values_.find(std::string(key));if(it==values_.end())return {false,SecretFault::NotFound,{},"Secret not found."};return {true,SecretFault::None,SecretBuffer(it->second),{}};}SecretDeleteResult erase(std::string_view key)override{if(!is_valid_secret_key(key))return {false,false,SecretFault::InvalidKey,"Invalid secret key."};if(next_fault!=SecretFault::None){const auto f=take_fault();return {false,false,f,"Injected secret-store failure."};}return {true,values_.erase(std::string(key))!=0,SecretFault::None,{}};}private:SecretFault take_fault(){const auto f=next_fault;next_fault=SecretFault::None;return f;}std::map<std::string,std::vector<std::byte>> values_{};};
}

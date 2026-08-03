#include "platform/secrets.hpp"
#include <algorithm>
namespace squiflow::platform {
namespace {void erase_bytes(std::vector<std::byte>& value)noexcept{volatile std::byte* p=value.data();for(std::size_t i=0;i<value.size();++i)p[i]=std::byte{0};value.clear();value.shrink_to_fit();}}
SecretBuffer::SecretBuffer(std::span<const std::byte> value):value_(value.begin(),value.end()){}
SecretBuffer::SecretBuffer(SecretBuffer&& other)noexcept:value_(std::move(other.value_)){}
SecretBuffer& SecretBuffer::operator=(SecretBuffer&& other)noexcept{if(this!=&other){clear();value_=std::move(other.value_);}return *this;}
SecretBuffer::~SecretBuffer(){clear();}
std::span<const std::byte> SecretBuffer::bytes()const noexcept{return value_;}
bool SecretBuffer::empty()const noexcept{return value_.empty();}
std::size_t SecretBuffer::size()const noexcept{return value_.size();}
void SecretBuffer::clear()noexcept{erase_bytes(value_);}
} // namespace squiflow::platform

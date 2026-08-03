#include "platform/secrets.hpp"

#include <cstring>
#include <mutex>
#include <new>
#include <sodium.h>
#include <stdexcept>
#include <utility>

namespace squiflow::platform {
namespace {

void initialize_sodium() {
    static std::once_flag once;
    static int result = -1;
    std::call_once(once, [] { result = sodium_init(); });
    if (result < 0) {
        throw std::runtime_error("Secure memory initialization failed.");
    }
}

} // namespace

SecretBuffer::SecretBuffer(std::span<const std::byte> value) {
    if (value.empty()) {
        return;
    }
    initialize_sodium();
    value_ = static_cast<std::byte*>(sodium_malloc(value.size()));
    if (value_ == nullptr) {
        throw std::bad_alloc();
    }
    size_ = value.size();
    std::memcpy(value_, value.data(), size_);
}

SecretBuffer::SecretBuffer(SecretBuffer&& other) noexcept
    : value_(std::exchange(other.value_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

SecretBuffer& SecretBuffer::operator=(SecretBuffer&& other) noexcept {
    if (this != &other) {
        clear();
        value_ = std::exchange(other.value_, nullptr);
        size_ = std::exchange(other.size_, 0);
    }
    return *this;
}

SecretBuffer::~SecretBuffer() { clear(); }

std::span<const std::byte> SecretBuffer::bytes() const noexcept {
    return {value_, size_};
}

bool SecretBuffer::empty() const noexcept { return size_ == 0; }
std::size_t SecretBuffer::size() const noexcept { return size_; }

void SecretBuffer::clear() noexcept {
    if (value_ != nullptr) {
        sodium_memzero(value_, size_);
        sodium_free(value_);
    }
    value_ = nullptr;
    size_ = 0;
}

} // namespace squiflow::platform

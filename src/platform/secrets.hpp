#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace squiflow::platform {

inline constexpr std::size_t kMaxSecretKeyLength = 64;
inline constexpr std::size_t kMaxSecretValueLength = 64U * 1024U;

enum class SecretFault {
    None,
    InvalidKey,
    EmptyValue,
    ValueTooLarge,
    NotFound,
    PermissionDenied,
    ProtectFailed,
    UnprotectFailed,
    CorruptEnvelope,
    UnsupportedVersion,
    ReadFailed,
    WriteFailed,
    ReplaceFailed,
    DeleteFailed
};

class SecretBuffer {
public:
    SecretBuffer() noexcept = default;
    explicit SecretBuffer(std::span<const std::byte> value);
    SecretBuffer(const SecretBuffer&) = delete;
    SecretBuffer& operator=(const SecretBuffer&) = delete;
    SecretBuffer(SecretBuffer&& other) noexcept;
    SecretBuffer& operator=(SecretBuffer&& other) noexcept;
    ~SecretBuffer();

    std::span<const std::byte> bytes() const noexcept;
    bool empty() const noexcept;
    std::size_t size() const noexcept;
    void clear() noexcept;

private:
    std::byte* value_{nullptr};
    std::size_t size_{0};
};

struct SecretWriteResult { bool ok{false}; SecretFault fault{SecretFault::None}; std::string message{}; };
struct SecretReadResult { bool ok{false}; SecretFault fault{SecretFault::None}; SecretBuffer value{}; std::string message{}; };
struct SecretDeleteResult { bool ok{false}; bool existed{false}; SecretFault fault{SecretFault::None}; std::string message{}; };

class SecretStore {
public:
    virtual ~SecretStore() = default;
    virtual SecretWriteResult store(std::string_view key, std::span<const std::byte> value) = 0;
    virtual SecretReadResult load(std::string_view key) = 0;
    virtual SecretDeleteResult erase(std::string_view key) = 0;
};

std::unique_ptr<SecretStore> make_secret_store(const std::string& secrets_directory);

} // namespace squiflow::platform

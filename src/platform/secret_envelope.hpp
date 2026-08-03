#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "platform/secrets.hpp"
namespace squiflow::platform {
inline constexpr std::size_t kSecretEnvelopeHeaderSize=12;
inline constexpr std::size_t kMaxProtectedSecretLength=1024U*1024U;
std::vector<std::byte> encode_secret_envelope(std::span<const std::byte> protected_value);
struct SecretEnvelopeResult{bool ok{false};SecretFault fault{SecretFault::CorruptEnvelope};std::span<const std::byte> protected_value{};};
SecretEnvelopeResult decode_secret_envelope(std::span<const std::byte> envelope)noexcept;
}

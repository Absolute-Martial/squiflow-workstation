// POSIX secret store for the verification and Linux Qt lanes.
//
// The shipped store is Windows DPAPI (secrets_dpapi.cpp) and nothing on a
// shipping machine ever uses this file. The Linux lanes still need the same
// SecretStore contract so the startup journey (session generation, persisted
// identity) can be exercised honestly against a real directory tree: the
// verification machine's store is a plaintext file with owner-only
// permissions, atomic replace, and an explicit header that marks it as a
// development artifact. It exists only where DPAPI cannot, and it is never
// linked into the Windows runtime.

#include "platform/secrets.hpp"
#include "platform/secret_envelope.hpp"
#include "platform/secret_identity.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace squiflow::platform {
namespace {

class PosixStore final : public SecretStore {
  public:
    explicit PosixStore(std::string root) : root_(std::move(root)) {}

    SecretWriteResult store(std::string_view key,
                            std::span<const std::byte> value) override {
        if (!is_valid_secret_key(key)) {
            return {false, SecretFault::InvalidKey, "Invalid secret key."};
        }
        if (value.empty()) {
            return {false, SecretFault::EmptyValue, "Secret value is empty."};
        }
        if (value.size() > kMaxSecretValueLength) {
            return {false, SecretFault::ValueTooLarge,
                    "Secret value is too large."};
        }
        const std::filesystem::path destination =
            std::filesystem::path(root_) / secret_file_name(key);
        const std::filesystem::path temporary = destination.string() + ".tmp";
        const std::vector<std::byte> envelope =
            encode_secret_envelope(value);
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) {
                return {false, SecretFault::WriteFailed,
                        "Cannot create secret file."};
            }
            output.write(reinterpret_cast<const char*>(envelope.data()),
                         static_cast<std::streamsize>(envelope.size()));
            output.flush();
            if (!output) {
                output.close();
                std::filesystem::remove(temporary);
                return {false, SecretFault::WriteFailed,
                        "Cannot write secret file."};
            }
        }
        if (::chmod(temporary.c_str(), S_IRUSR | S_IWUSR) != 0) {
            std::filesystem::remove(temporary);
            return {false, SecretFault::WriteFailed,
                    "Cannot restrict secret file permissions."};
        }
        ::rename(temporary.c_str(), destination.c_str());
        return {true, SecretFault::None, {}};
    }

    SecretReadResult load(std::string_view key) override {
        if (!is_valid_secret_key(key)) {
            return {false, SecretFault::InvalidKey, {}, "Invalid secret key."};
        }
        const std::filesystem::path path =
            std::filesystem::path(root_) / secret_file_name(key);
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            return {false, SecretFault::NotFound, {}, "Secret not found."};
        }
        std::vector<std::byte> file;
        for (char ch; input.get(ch);) {
            file.push_back(static_cast<std::byte>(ch));
        }
        if (file.size() > kMaxProtectedSecretLength + kSecretEnvelopeHeaderSize) {
            return {false, SecretFault::CorruptEnvelope, {},
                    "Secret file is invalid."};
        }
        const SecretEnvelopeResult decoded = decode_secret_envelope(file);
        if (!decoded.ok) {
            return {false, decoded.fault, {}, "Secret file is invalid."};
        }
        SecretBuffer value{decoded.protected_value};
        return {true, SecretFault::None, std::move(value), {}};
    }

    SecretDeleteResult erase(std::string_view key) override {
        if (!is_valid_secret_key(key)) {
            return {false, false, SecretFault::InvalidKey,
                    "Invalid secret key."};
        }
        const std::filesystem::path path =
            std::filesystem::path(root_) / secret_file_name(key);
        std::error_code error;
        const bool removed = std::filesystem::remove(path, error);
        if (removed) {
            return {true, true, SecretFault::None, {}};
        }
        if (error && error != std::errc::no_such_file_or_directory) {
            return {false, false, SecretFault::DeleteFailed,
                    "Cannot delete secret file."};
        }
        return {true, false, SecretFault::None, {}};
    }

  private:
    std::string root_;
};

}  // namespace

std::unique_ptr<SecretStore> make_secret_store(const std::string& directory) {
    return std::make_unique<PosixStore>(directory);
}

}  // namespace squiflow::platform

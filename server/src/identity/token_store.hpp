#pragma once

// Portable half of Phase 8.3 (Identity and tokens). This header has no Hical
// and no PostgreSQL dependency on purpose. Hical is now selected as the inbound
// HTTP adapter, but token issuance/validation rules do not belong to an HTTP
// framework or database driver. The PostgreSQL-backed TokenStore is deferred
// to 8.2/8.3; only the durable storage adapter is missing here, not the policy.

#include "engine/records/identity.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace squiflow::server::identity {

// A stable, non-secret lookup key for a token record. Safe to log, index, and
// send back to a client alongside the secret. Never sufficient by itself to
// authenticate -- see TokenSecret.
struct TokenId {
    std::string value;

    friend bool operator==(const TokenId&, const TokenId&) = default;
};

// The bearer secret. High-entropy and opaque: it is handed to the caller
// exactly once, at issuance, and is never itself persisted -- only its hash
// is (see TokenRecord::secret_hash).
struct TokenSecret {
    std::string value;
};

struct TokenRecord {
    TokenId id;
    engine::PersonId person;
    engine::DeviceId device;
    std::string secret_hash;
    engine::Timestamp issued_at;
    engine::Timestamp expires_at;
    bool revoked = false;
};

// Storage seam. The real (8.2/8.3) implementation is PostgreSQL-backed; this
// header only declares the contract every implementation, real or fake, must
// satisfy: revocation is visible to the very next find(), never eventually.
class TokenStore {
public:
    virtual ~TokenStore() = default;

    virtual void put(const TokenRecord& record) = 0;
    virtual std::optional<TokenRecord> find(const TokenId& id) const = 0;
    virtual void mark_revoked(const TokenId& id) = 0;
};

// A thread-safe, in-process TokenStore. Used by tests here, and usable as a
// real (if non-durable) store for a single-process server deployment before
// 8.2's PostgreSQL-backed store exists.
class InMemoryTokenStore final : public TokenStore {
public:
    void put(const TokenRecord& record) override;
    std::optional<TokenRecord> find(const TokenId& id) const override;
    void mark_revoked(const TokenId& id) override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TokenRecord> records_;
};

}  // namespace squiflow::server::identity

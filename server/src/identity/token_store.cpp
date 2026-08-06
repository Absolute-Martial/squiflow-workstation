#include "identity/token_store.hpp"

namespace squiflow::server::identity {

void InMemoryTokenStore::put(const TokenRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    records_[record.id.value] = record;
}

std::optional<TokenRecord> InMemoryTokenStore::find(const TokenId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = records_.find(id.value);
    if (found == records_.end()) {
        return std::nullopt;
    }
    return found->second;
}

void InMemoryTokenStore::mark_revoked(const TokenId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = records_.find(id.value);
    if (found != records_.end()) {
        found->second.revoked = true;
    }
}

}  // namespace squiflow::server::identity

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::receivables {

class ReceivablesService {
public:
    using Clock = std::function<std::int64_t()>;

    explicit ReceivablesService(Clock clock) : clock_{std::move(clock)} {}

    void invoice_draft_create(engine::Transaction& transaction, const Call& call) const;
    void invoice_draft_update(engine::Transaction& transaction, const Call& call) const;
    void invoice_draft_discard(engine::Transaction& transaction, const Call& call) const;
    void payment_allocate(engine::Transaction& transaction, const Call& call) const;
    void credit_account_set(engine::Transaction& transaction, const Call& call) const;
    std::vector<engine::Row> statement_prepare(const engine::Store& store,
                                               const Call& call) const;
    void statement_send(engine::Transaction& transaction, const Call& call) const;
    std::vector<engine::Row> document_print(const engine::Store& store,
                                            const Call& call) const;

private:
    Clock clock_;
};

}  // namespace squiflow::modules::receivables

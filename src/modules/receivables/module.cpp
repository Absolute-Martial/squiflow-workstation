#include "modules/receivables/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "modules/receivables/data/tables.hpp"
#include "modules/receivables/service/receivables_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::receivables {
namespace {

class ReceivablesModule final : public Module {
public:
    explicit ReceivablesModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override {
        return protocol::ModuleId::receivables;
    }

    std::vector<engine::Migration> migrations() const override {
        return tables::migrations();
    }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::invoice_draft_create,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.invoice_draft_create(transaction, call);
            });
        registry.on_write(protocol::OperationId::invoice_draft_update,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.invoice_draft_update(transaction, call);
            });
        registry.on_write(protocol::OperationId::invoice_draft_discard,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.invoice_draft_discard(transaction, call);
            });
        registry.on_write(protocol::OperationId::payment_allocate,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.payment_allocate(transaction, call);
            });
        registry.on_write(protocol::OperationId::credit_account_set,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.credit_account_set(transaction, call);
            });
        registry.on_read(protocol::OperationId::statement_prepare,
            [this](const engine::Store& store, const Call& call) {
                return service_.statement_prepare(store, call);
            });
        registry.on_write(protocol::OperationId::statement_send,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.statement_send(transaction, call);
            });
        registry.on_read(protocol::OperationId::document_print,
            [this](const engine::Store& store, const Call& call) {
                return service_.document_print(store, call);
            });
    }

private:
    ReceivablesService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) throw std::logic_error("receivables needs a clock");
    return std::make_unique<ReceivablesModule>(std::move(clock));
}

}  // namespace squiflow::modules::receivables

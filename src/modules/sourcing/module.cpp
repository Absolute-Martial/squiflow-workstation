#include "modules/sourcing/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "modules/registry.hpp"
#include "modules/sourcing/data/tables.hpp"
#include "modules/sourcing/service/sourcing_service.hpp"

namespace squiflow::modules::sourcing {
namespace {

class SourcingModule final : public Module {
public:
    explicit SourcingModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override {
        return protocol::ModuleId::sourcing;
    }

    std::vector<engine::Migration> migrations() const override {
        return tables::migrations();
    }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::supplier_create,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.create_supplier(transaction, call);
            });
        registry.on_write(protocol::OperationId::supplier_update,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.update_supplier(transaction, call);
            });
        registry.on_write(protocol::OperationId::purchase_settle,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.settle(transaction, call);
            });
        registry.on_read(protocol::OperationId::purchase_lookup,
            [this](const engine::Store& store, const Call& call) {
                return service_.lookup(store, call);
            });
    }

private:
    SourcingService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) {
        throw std::logic_error("sourcing needs a clock");
    }
    return std::make_unique<SourcingModule>(std::move(clock));
}

}  // namespace squiflow::modules::sourcing

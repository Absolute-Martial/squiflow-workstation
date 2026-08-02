#include "modules/agreements/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "modules/agreements/data/tables.hpp"
#include "modules/agreements/service/agreements_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::agreements {
namespace {

class AgreementsModule final : public Module {
public:
    explicit AgreementsModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override {
        return protocol::ModuleId::agreements;
    }

    std::vector<engine::Migration> migrations() const override {
        return tables::migrations();
    }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::agreement_create,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.create(transaction, call);
            });
        registry.on_write(protocol::OperationId::agreement_update,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.update(transaction, call);
            });
        registry.on_write(protocol::OperationId::agreement_close,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.close(transaction, call);
            });
        registry.on_write(protocol::OperationId::agreement_reopen,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.reopen(transaction, call);
            });
    }

private:
    AgreementsService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) {
        throw std::logic_error("agreements needs a clock");
    }
    return std::make_unique<AgreementsModule>(std::move(clock));
}

}  // namespace squiflow::modules::agreements

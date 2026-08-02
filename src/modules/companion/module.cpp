#include "modules/companion/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "modules/companion/data/tables.hpp"
#include "modules/companion/service/companion_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::companion {
namespace {
class CompanionModule final : public Module {
public:
    explicit CompanionModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override { return protocol::ModuleId::companion; }
    std::vector<engine::Migration> migrations() const override { return tables::migrations(); }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::task_create,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.create(transaction, call);
            });
        registry.on_write(protocol::OperationId::task_update,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.update(transaction, call);
            });
        registry.on_write(protocol::OperationId::task_complete,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.complete(transaction, call);
            });
        registry.on_write(protocol::OperationId::task_snooze,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.snooze(transaction, call);
            });
    }

private:
    CompanionService service_;
};
}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) throw std::logic_error("companion needs a clock");
    return std::make_unique<CompanionModule>(std::move(clock));
}

}  // namespace squiflow::modules::companion

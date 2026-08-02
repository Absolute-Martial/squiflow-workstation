#include "modules/pricing/module.hpp"

#include <memory>
#include <stdexcept>

#include "modules/pricing/data/tables.hpp"
#include "modules/pricing/service/pricing_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::pricing {
namespace {

class PricingModule final : public Module {
public:
    explicit PricingModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override { return protocol::ModuleId::pricing; }
    std::vector<engine::Migration> migrations() const override { return tables::migrations(); }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::rate_set,
            [this](engine::Transaction& tx, const Call& call) {
                service_.set_rate(tx, call); });
        registry.on_write(protocol::OperationId::rate_remove,
            [this](engine::Transaction& tx, const Call& call) {
                service_.remove_rate(tx, call); });
        registry.on_write(protocol::OperationId::rate_override,
            [this](engine::Transaction& tx, const Call& call) {
                service_.override_rate(tx, call); });
        registry.on_write(protocol::OperationId::rate_default_set,
            [this](engine::Transaction& tx, const Call& call) {
                service_.set_default_rate(tx, call); });
    }

private:
    PricingService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) throw std::logic_error("pricing needs a clock");
    return std::make_unique<PricingModule>(std::move(clock));
}

}  // namespace squiflow::modules::pricing

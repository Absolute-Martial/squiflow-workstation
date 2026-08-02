#include "modules/parties/module.hpp"

#include <memory>
#include <stdexcept>

#include "modules/parties/data/tables.hpp"
#include "modules/parties/service/parties_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::parties {
namespace {

class PartiesModule final : public Module {
public:
    explicit PartiesModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override { return protocol::ModuleId::parties; }
    std::vector<engine::Migration> migrations() const override { return tables::migrations(); }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::party_create,
            [this](engine::Transaction& tx, const Call& call) {
                service_.create_party(tx, call); });
        registry.on_write(protocol::OperationId::party_update,
            [this](engine::Transaction& tx, const Call& call) {
                service_.update_party(tx, call); });
        registry.on_write(protocol::OperationId::party_archive,
            [this](engine::Transaction& tx, const Call& call) {
                service_.archive_party(tx, call); });
        registry.on_write(protocol::OperationId::party_terms_set,
            [this](engine::Transaction& tx, const Call& call) {
                service_.set_terms(tx, call); });
        registry.on_write(protocol::OperationId::party_contact_add,
            [this](engine::Transaction& tx, const Call& call) {
                service_.add_contact(tx, call); });
    }

private:
    PartiesService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) throw std::logic_error("parties needs a clock");
    return std::make_unique<PartiesModule>(std::move(clock));
}

}  // namespace squiflow::modules::parties

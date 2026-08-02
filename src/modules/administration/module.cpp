#include "modules/administration/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "modules/administration/data/repository.hpp"
#include "modules/administration/data/tables.hpp"
#include "modules/administration/service/administration_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::administration {
namespace {

class AdministrationModule final : public Module {
public:
    explicit AdministrationModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override { return protocol::ModuleId::administration; }

    std::vector<engine::Migration> migrations() const override { return tables::migrations(); }

    void install(Registry& registry) override {
        // Every operation the protocol declares for administration is listed
        // here, once. Anything missing fails at startup rather than showing a
        // person a button that does nothing.
        registry.on_write(protocol::OperationId::person_create,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.create_person(transaction, call);
                          });
        registry.on_write(protocol::OperationId::person_update,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.update_person(transaction, call);
                          });
        registry.on_write(protocol::OperationId::person_disable,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.disable_person(transaction, call);
                          });
        registry.on_write(protocol::OperationId::right_grant,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.grant_right(transaction, call);
                          });
        registry.on_write(protocol::OperationId::right_revoke,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.revoke_right(transaction, call);
                          });
        registry.on_write(protocol::OperationId::device_register,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.register_device(transaction, call);
                          });
        registry.on_write(protocol::OperationId::device_retire,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.retire_device(transaction, call);
                          });
        registry.on_write(protocol::OperationId::shop_setting_update,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.update_setting(transaction, call);
                          });
        registry.on_write(protocol::OperationId::module_activation_set,
                          [this](engine::Transaction& transaction, const Call& call) {
                              service_.set_activation(transaction, call);
                          });
        registry.on_read(protocol::OperationId::audit_export,
                         [this](const engine::Store& store, const Call& call) {
                             return service_.export_audit(store, call);
                         });
    }

private:
    AdministrationService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) {
        throw std::logic_error("administration needs a clock");
    }
    return std::make_unique<AdministrationModule>(std::move(clock));
}

}  // namespace squiflow::modules::administration

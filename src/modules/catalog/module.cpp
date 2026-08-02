#include "modules/catalog/module.hpp"

#include <memory>
#include <stdexcept>

#include "modules/catalog/data/tables.hpp"
#include "modules/catalog/service/catalog_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::catalog {
namespace {

class CatalogModule final : public Module {
public:
    explicit CatalogModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override { return protocol::ModuleId::catalog; }
    std::vector<engine::Migration> migrations() const override { return tables::migrations(); }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::product_create,
            [this](engine::Transaction& tx, const Call& call) {
                service_.create_product(tx, call); });
        registry.on_write(protocol::OperationId::product_update,
            [this](engine::Transaction& tx, const Call& call) {
                service_.update_product(tx, call); });
        registry.on_write(protocol::OperationId::product_archive,
            [this](engine::Transaction& tx, const Call& call) {
                service_.archive_product(tx, call); });
    }

private:
    CatalogService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) throw std::logic_error("catalog needs a clock");
    return std::make_unique<CatalogModule>(std::move(clock));
}

}  // namespace squiflow::modules::catalog

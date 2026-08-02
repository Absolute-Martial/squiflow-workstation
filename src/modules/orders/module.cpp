#include "modules/orders/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "modules/orders/data/tables.hpp"
#include "modules/orders/service/orders_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::orders {
namespace {

class OrdersModule final : public Module {
public:
    explicit OrdersModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override {
        return protocol::ModuleId::orders;
    }

    std::vector<engine::Migration> migrations() const override {
        return tables::migrations();
    }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::order_create,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.create(transaction, call);
            });
        registry.on_write(protocol::OperationId::order_update,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.update(transaction, call);
            });
        registry.on_write(protocol::OperationId::order_line_add,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.add_line(transaction, call);
            });
        registry.on_write(protocol::OperationId::order_cancel,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.cancel(transaction, call);
            });
    }

private:
    OrdersService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) {
        throw std::logic_error("orders needs a clock");
    }
    return std::make_unique<OrdersModule>(std::move(clock));
}

}  // namespace squiflow::modules::orders

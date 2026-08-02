#include "modules/quotations/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "modules/quotations/data/tables.hpp"
#include "modules/quotations/service/quotations_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::quotations {
namespace {

class QuotationsModule final : public Module {
public:
    explicit QuotationsModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override {
        return protocol::ModuleId::quotations;
    }

    std::vector<engine::Migration> migrations() const override {
        return tables::migrations();
    }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::quotation_create,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.create(transaction, call);
            });
        registry.on_write(protocol::OperationId::quotation_revise,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.revise(transaction, call);
            });
        registry.on_write(protocol::OperationId::quotation_issue,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.issue(transaction, call);
            });
        registry.on_write(protocol::OperationId::quotation_accept,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.accept(transaction, call);
            });
        registry.on_write(protocol::OperationId::quotation_expire,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.expire(transaction, call);
            });
    }

private:
    QuotationsService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) {
        throw std::logic_error("quotations needs a clock");
    }
    return std::make_unique<QuotationsModule>(std::move(clock));
}

}  // namespace squiflow::modules::quotations

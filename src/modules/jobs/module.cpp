#include "modules/jobs/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "modules/jobs/data/tables.hpp"
#include "modules/jobs/service/jobs_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::jobs {
namespace {

class JobsModule final : public Module {
public:
    explicit JobsModule(std::function<std::int64_t()> clock)
        : service_(std::move(clock)) {}

    protocol::ModuleId id() const noexcept override {
        return protocol::ModuleId::jobs;
    }

    std::vector<engine::Migration> migrations() const override {
        return tables::migrations();
    }

    void install(Registry& registry) override {
        registry.on_write(protocol::OperationId::job_create,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.create(transaction, call);
            });
        registry.on_write(protocol::OperationId::job_update,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.update(transaction, call);
            });
        registry.on_write(protocol::OperationId::job_state_change,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.state_change(transaction, call);
            });
        registry.on_write(protocol::OperationId::job_cancel,
            [this](engine::Transaction& transaction, const Call& call) {
                service_.cancel(transaction, call);
            });
    }

private:
    JobsService service_;
};

}  // namespace

ModulePtr make_module(std::function<std::int64_t()> clock) {
    if (!clock) {
        throw std::logic_error("jobs needs a clock");
    }
    return std::make_unique<JobsModule>(std::move(clock));
}

}  // namespace squiflow::modules::jobs

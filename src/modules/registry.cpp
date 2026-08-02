#include "modules/registry.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <utility>

#include "engine/audit/audit_log.hpp"

namespace squiflow::modules {

namespace {

std::string operation_name(protocol::OperationId operation) {
    const auto* info = protocol::find_operation(static_cast<std::uint32_t>(operation));
    return info == nullptr ? "unknown operation" : std::string(info->name);
}
std::vector<protocol::ModuleId> required_modules(protocol::OperationId operation) {
    using M=protocol::ModuleId; using O=protocol::OperationId;
    switch(operation) {
    case O::quote_to_order:return {M::quotations,M::orders,M::pricing};
    case O::order_to_jobs:return {M::orders,M::jobs};
    case O::issue_invoice:return {M::agreements,M::receivables,M::orders,M::pricing};
    case O::cancel_and_reissue:return {M::agreements,M::receivables};
    case O::apply_agreement:return {M::agreements,M::pricing,M::receivables};
    case O::take_payment:return {M::receivables,M::parties};
    case O::counter_sale:return {M::orders,M::pricing,M::receivables};
    case O::record_purchase:return {M::sourcing};
    case O::prepare_document_delivery:return {M::parties,M::receivables,M::quotations,M::agreements};
    case O::request_document_delivery:return {M::parties,M::receivables,M::quotations,M::agreements};
    default:return {};
    }
}
bool blank(const std::string& text) noexcept { return std::all_of(text.begin(),text.end(),[](char c){return static_cast<unsigned char>(c)<=static_cast<unsigned char>(' ');}); }

std::string module_name_of(protocol::ModuleId module) {
    return std::string(protocol::module_name(module));
}

}  // namespace

RegistryError::RegistryError(const std::string& message) : std::runtime_error(message) {}

Module::~Module() = default;

Registry::Registry(Clock clock) : clock_(std::move(clock)), outbox_(clock_) {
    if (!clock_) {
        throw RegistryError("the registry needs a clock");
    }
    // Everything on, until something is deliberately switched off.
    set_disabled({});
}

Registry::~Registry() = default;

void Registry::add(ModulePtr module) {
    if (!module) {
        throw RegistryError("a null module was registered");
    }
    const protocol::ModuleId id = module->id();
    if (has(id)) {
        throw RegistryError("module " + module_name_of(id) + " is registered twice");
    }

    modules_.push_back(std::move(module));

    // install() may only register operations belonging to this module, and the
    // registry decides which module that is rather than asking.
    installing_ = true;
    installing_module_ = id;
    try {
        modules_.back()->install(*this);
    } catch (...) {
        installing_ = false;
        throw;
    }
    installing_ = false;
}

bool Registry::has(protocol::ModuleId module) const noexcept {
    return std::any_of(modules_.begin(), modules_.end(),
                       [module](const ModulePtr& m) { return m->id() == module; });
}

std::vector<protocol::ModuleId> Registry::registered() const {
    std::vector<protocol::ModuleId> ids;
    ids.reserve(modules_.size());
    for (const ModulePtr& m : modules_) {
        ids.push_back(m->id());
    }
    return ids;
}

std::size_t Registry::size() const noexcept { return modules_.size(); }

void Registry::register_handler(protocol::OperationId operation, Handler handler) {
    if (protocol::is_workflow_operation(operation)) throw RegistryError("workflow " + operation_name(operation) + " cannot be installed by a module");
    if (!installing_) {
        throw RegistryError("handler for " + operation_name(operation) +
                            " was registered outside install()");
    }

    const protocol::OperationInfo& info = protocol::operation(operation);
    if (info.module != installing_module_) {
        // A module reaching into another module's operations is the first step
        // of the layering coming apart, and it is invisible at run time.
        throw RegistryError("module " + module_name_of(installing_module_) +
                            " tried to handle " + operation_name(operation) +
                            ", which belongs to " + module_name_of(info.module));
    }

    if (handlers_.find(operation) != handlers_.end()) {
        throw RegistryError("two handlers for " + operation_name(operation));
    }

    handler.owner = installing_module_;
    handlers_.emplace(operation, std::move(handler));
}

void Registry::on_write(protocol::OperationId operation, WriteHandler handler) {
    if (!handler) {
        throw RegistryError("empty write handler for " + operation_name(operation));
    }
    Handler entry;
    entry.kind = Kind::Write;
    entry.write = std::move(handler);
    register_handler(operation, std::move(entry));
}

void Registry::on_read(protocol::OperationId operation, ReadHandler handler) {
    if (!handler) {
        throw RegistryError("empty read handler for " + operation_name(operation));
    }

    // A read that would be sent to the server is a contradiction: there is
    // nothing to send. Catching it here is cheaper than wondering later why an
    // outbox entry has no effect.
    const protocol::OperationInfo& info = protocol::operation(operation);
    if (info.sync_class == protocol::OperationClass::Synchronizable) {
        throw RegistryError(operation_name(operation) +
                            " is synchronisable and cannot be a read handler");
    }

    Handler entry;
    entry.kind = Kind::Read;
    entry.read = std::move(handler);
    register_handler(operation, std::move(entry));
}

void Registry::install_workflow(workflows::WorkflowDefinition definition) {
    if (!protocol::is_workflow_operation(definition.operation)) throw RegistryError(operation_name(definition.operation)+" is not a workflow operation");
    if (!definition.handler) throw RegistryError("empty workflow handler for "+operation_name(definition.operation));
    if (definition.requirements.empty()) throw RegistryError("workflow "+operation_name(definition.operation)+" has no module requirements");
    if (workflows_.contains(definition.operation)||handlers_.contains(definition.operation)) throw RegistryError("two handlers for "+operation_name(definition.operation));
    std::set<protocol::ModuleId> unique;
    for(auto module:definition.requirements) {
        if(!protocol::is_valid(module)) throw RegistryError("workflow names an invalid module requirement");
        if(!unique.insert(module).second) throw RegistryError("workflow repeats module "+module_name_of(module));
        if(!has(module)) throw RegistryError("workflow requires unregistered module "+module_name_of(module));
    }
    const auto owner=protocol::operation(definition.operation).module;
    if(!unique.contains(owner)) throw RegistryError("workflow omits its protocol owner "+module_name_of(owner));
    const auto expected=required_modules(definition.operation); const std::set<protocol::ModuleId> expected_set(expected.begin(),expected.end());
    if(unique!=expected_set) throw RegistryError("workflow "+operation_name(definition.operation)+" does not declare its exact protocol requirements");
    workflows_.emplace(definition.operation,WorkflowEntry{std::move(definition.requirements),std::move(definition.handler)});
}
bool Registry::workflow_available(protocol::OperationId operation) const noexcept {
    const auto found=workflows_.find(operation); if(found==workflows_.end()) return false;
    return std::all_of(found->second.requirements.begin(),found->second.requirements.end(),[this](auto m){return has(m)&&active(m);});
}
bool Registry::handled(protocol::OperationId operation) const noexcept { return protocol::is_workflow_operation(operation)?workflow_available(operation):handlers_.contains(operation); }
std::vector<std::string> Registry::unhandled() const {
    std::vector<std::string> missing;
    for(const auto& info:protocol::all_operations()) if(!protocol::is_workflow_operation(info.id)&&has(info.module)&&!handlers_.contains(info.id)) missing.emplace_back(info.name);
    for(auto op:protocol::workflow_operations()){const auto& info=protocol::operation(op);if(has(info.module)&&!workflows_.contains(op))missing.emplace_back(info.name);} return missing;
}

void Registry::require_complete() const {
    const std::vector<std::string> missing = unhandled();
    if (missing.empty()) {
        return;
    }
    std::string message = "operations declared with no handler:";
    for (const std::string& name : missing) {
        message += " " + name;
    }
    throw RegistryError(message);
}

void Registry::collect_migrations(engine::MigrationRunner& runner) const {
    std::vector<engine::Migration> all;
    std::set<int> numbers;

    // Numbers below kFirstModuleMigration belong to the engine. The outbox,
    // the sync cursors and the conflict log are not any module's property -
    // every module's changes travel through them - so no module gets to own
    // the migration that creates them.
    engine::Migration engine_tables;
    engine_tables.number = 1;
    engine_tables.name = "engine tables";
    engine_tables.schema = [](engine::Store& store) {
        engine::Outbox::define(store);
        engine::Cursor::define(store);
        engine::ConflictLog::define(store);
    };
    numbers.insert(engine_tables.number);
    all.push_back(std::move(engine_tables));

    engine::Migration audit_table{22, "workflow audit log", [](engine::Store& store) { engine::AuditLog::define(store); }, {}};
    numbers.insert(22); all.push_back(std::move(audit_table));

    for (const ModulePtr& m : modules_) {
        std::vector<engine::Migration> mine = m->migrations();
        for (engine::Migration& migration : mine) {
            if (migration.number < kFirstModuleMigration) {
                throw RegistryError("module " + module_name_of(m->id()) + " claims migration " +
                                    std::to_string(migration.number) + "; numbers below " +
                                    std::to_string(kFirstModuleMigration) +
                                    " belong to the engine");
            }
            if (!numbers.insert(migration.number).second) {
                // Numbering is global on purpose. Two modules both claiming 7
                // would leave the order of application to whichever was
                // registered first, which is not a decision anyone made.
                throw RegistryError("migration number " + std::to_string(migration.number) +
                                    " is claimed twice; numbering is global");
            }
            all.push_back(std::move(migration));
        }
    }

    std::sort(all.begin(), all.end(),
              [](const engine::Migration& a, const engine::Migration& b) {
                  return a.number < b.number;
              });

    for (engine::Migration& migration : all) {
        runner.add(std::move(migration));
    }
}

void Registry::set_disabled(const std::vector<protocol::ModuleId>& disabled) {
    protocol::ActivationResult result = protocol::resolve_activation(disabled);
    if (!result.ok) {
        throw RegistryError(result.error);
    }
    activation_ = result.activation;
}

bool Registry::active(protocol::ModuleId module) const noexcept {
    return activation_.is_active(module);
}

Outcome Registry::refuse(const engine::Decision& decision) {
    Outcome outcome;
    outcome.ok = false;
    outcome.reason = decision.reason;
    outcome.error = decision.explanation;
    return outcome;
}

Outcome Registry::run(engine::Database& database, const Call& call, const engine::Session& session, engine::ConnectionState connection) {
    const bool workflow_op=protocol::is_workflow_operation(call.operation); const auto ordinary=handlers_.find(call.operation); const auto workflow=workflows_.find(call.operation);
    if((workflow_op&&workflow==workflows_.end())||(!workflow_op&&ordinary==handlers_.end())) throw RegistryError("no handler for "+operation_name(call.operation));
    const auto& info=protocol::operation(call.operation); const auto decision=engine::may_run(call.operation,session,connection,activation_); if(!decision.allowed)return refuse(decision);
    if(workflow_op) for(auto requirement:workflow->second.requirements){if(!has(requirement))throw RegistryError("workflow lost required module");if(!active(requirement))return refuse({false,engine::DenialReason::ModuleInactive,"A required module is switched off, so this workflow is unavailable."});}
    Call effective=call; effective.actor=&session;
    if(!workflow_op&&ordinary->second.kind==Kind::Read){Outcome outcome;outcome.ok=true;try{database.read([&](const engine::Store& store){outcome.rows=ordinary->second.read(store,effective);});}catch(const RuleViolation& e){return {false,engine::DenialReason::None,e.what(),{},false,false};}return outcome;}
    const bool synchronised=info.sync_class==protocol::OperationClass::Synchronizable;
    if(synchronised){if(call.record_id.empty())throw RegistryError(operation_name(call.operation)+" will be sent and so needs a record to be ordered against");if(call.idempotency_key.empty())throw RegistryError(operation_name(call.operation)+" will be sent and so needs an idempotency key");}else if(!call.idempotency_key.empty())throw RegistryError(operation_name(call.operation)+" is never sent, so an idempotency key means something is confused");
    bool queued=false,replayed=false;
    try{database.write([&](engine::Transaction& transaction){if(synchronised&&transaction.find(engine::Outbox::table_name(),call.idempotency_key)){replayed=true;return;}if(workflow_op){const auto result=workflow->second.handler(transaction,effective);if(!result.audit_subject.is_valid()||!protocol::is_valid(result.audit_subject.module))throw RuleViolation("the workflow produced an invalid audit subject");if(blank(result.audit_detail))throw RuleViolation("the workflow produced no audit detail");engine::AuditEntry audit;audit.id=engine::AuditLog::id_for(call.idempotency_key);audit.operation=call.operation;audit.person=session.person;audit.device=session.device;audit.at.ms=clock_();audit.subject=result.audit_subject;audit.detail=result.audit_detail;engine::AuditLog::record(transaction,call.idempotency_key,audit);}else ordinary->second.write(transaction,effective);if(synchronised){engine::OutboxEntry entry;entry.idempotency_key=call.idempotency_key;entry.operation=call.operation;entry.record_id=call.record_id;entry.payload=call.payload;queued=outbox_.enqueue(transaction,entry)==engine::EnqueueResult::Enqueued;}});}catch(const RuleViolation& e){return {false,engine::DenialReason::None,e.what(),{},false,false};}
    Outcome outcome;outcome.ok=true;outcome.queued=queued;outcome.replayed=replayed;return outcome;
}

}  // namespace squiflow::modules

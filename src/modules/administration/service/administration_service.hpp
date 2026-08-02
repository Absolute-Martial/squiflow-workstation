#pragma once

// The rules about people, rights, devices, settings and switched-off modules.
//
// Everything here is written against a transaction it did not open and cannot
// commit. It refuses by throwing RuleViolation, which rolls the transaction
// back on the way out, so a rule discovered halfway through a change cannot
// leave the first half of that change behind.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::administration {

class AdministrationService {
public:
    using Clock = std::function<std::int64_t()>;

    explicit AdministrationService(Clock clock);

    void create_person(engine::Transaction& transaction, const Call& call);
    void update_person(engine::Transaction& transaction, const Call& call);
    void disable_person(engine::Transaction& transaction, const Call& call);

    void grant_right(engine::Transaction& transaction, const Call& call);
    void revoke_right(engine::Transaction& transaction, const Call& call);

    void register_device(engine::Transaction& transaction, const Call& call);
    void retire_device(engine::Transaction& transaction, const Call& call);

    void update_setting(engine::Transaction& transaction, const Call& call);
    void set_activation(engine::Transaction& transaction, const Call& call);

    std::vector<engine::Row> export_audit(const engine::Store& store, const Call& call) const;

private:
    void record(engine::Transaction& transaction, const Call& call, const std::string& summary);

    Clock clock_;
};

// Reads the payload, or refuses in words a person can act on. A payload that
// cannot be decoded is not a crash and not a silent empty request.
engine::Row read_fields(const Call& call);

// The signed-in person, as the registry filled it in.
const engine::Session& actor(const Call& call);

}  // namespace squiflow::modules::administration

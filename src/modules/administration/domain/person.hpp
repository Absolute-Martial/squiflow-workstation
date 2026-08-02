#pragma once

// A person who may sign in, and what they are allowed to do.
//
// There are no roles. The shopkeeper grants each right to each person
// directly, which is what was asked for and is also the only thing that is
// honest at this size: a "role" with one member is a layer of indirection
// pretending to be a policy.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <squiflow/protocol/right_id.hpp>

#include "engine/storage/store.hpp"

namespace squiflow::modules::administration {

struct Person {
    std::string id{};
    std::string display_name{};

    // Stored already normalised. Comparing usernames is done on this value and
    // never on what was typed.
    std::string username{};

    // Hashed by the platform layer before it ever reaches this module. Nothing
    // here has ever seen the password itself, which is the point.
    std::string password_hash{};

    bool is_owner{false};
    bool disabled{false};

    std::int64_t created_at{0};
    std::int64_t updated_at{0};
    std::string created_by{};
};

// Trims, folds ASCII letters to lower case, and leaves everything else alone.
//
// Deliberately not clever about Unicode. A username that differs from another
// only by an accent is a problem this shop will not have, and a case-folding
// implementation that is wrong for Devanagari would be worse than one that
// leaves it untouched.
std::string normalise_username(std::string_view raw);

std::string trim(std::string_view raw);

// Throws RuleViolation with something a person can act on.
void validate(const Person& person);

engine::Row to_row(const Person& person);
Person person_from_row(const engine::Row& row);

// Rights are stored by name, not by number. The numbers come from the order of
// lines in rights.def, so storing them would mean that inserting a right in
// the middle of that file silently re-assigns everyone's permissions.
std::optional<protocol::RightId> right_from_name(std::string_view name);

}  // namespace squiflow::modules::administration

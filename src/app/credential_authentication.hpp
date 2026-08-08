#pragma once

// Turning a typed username and password into a signed-in Session.
//
// This is the one place in the application that is allowed to hold a
// plaintext password: it reads it, verifies it, and lets it go out of scope.
// Everything downstream of here (administration, the command gateway, sync)
// only ever sees a Session or an Argon2id hash. See
// modules/administration/domain/person.hpp for the module-side half of that
// boundary.

#include <cstdint>
#include <string>
#include <string_view>

#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::app {

enum class LoginFault : std::uint8_t {
    None,

    // Either the username does not exist or the password was wrong. The two
    // are never told apart: doing so would let anyone standing at the shop's
    // own terminal learn which usernames are real, and the shopkeeper never
    // needed that distinction to correct a typo.
    InvalidCredentials,

    // The username and password were both correct, but the account has been
    // switched off. Told apart from InvalidCredentials on purpose: the
    // account's disabled state is not a secret (it is a plain field on the
    // person's own administration record), and a shopkeeper staring at
    // "invalid credentials" for an account they just disabled themselves is
    // not being protected by the ambiguity, only confused by it.
    PersonDisabled,
};

struct LoginResult {
    bool ok{false};
    engine::Session session{};
    LoginFault fault{LoginFault::None};

    // Safe to show to whoever just typed the password: never includes the
    // password itself, a hash, or which half of the credential pair failed.
    std::string message{};
};

// Verifies a username and password against the administration person table
// and, on success, builds the Session this device should run under for the
// rest of the process lifetime.
//
// `device` is carried through unchanged into the returned Session; this
// function does not look up or validate the device record, because a device
// that cannot be found is a separate, earlier failure (see
// RealStartupServices) and not a login failure.
LoginResult authenticate(const engine::Store& store, const engine::DeviceId& device,
                          std::string_view username, std::string_view password);

}  // namespace squiflow::app

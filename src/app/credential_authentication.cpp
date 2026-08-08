#include "app/credential_authentication.hpp"

#include <optional>

#include "modules/administration/data/repository.hpp"
#include "modules/administration/domain/person.hpp"
#include "platform/password_hash.hpp"

namespace squiflow::app {

namespace {

namespace admin = modules::administration;

LoginResult invalid_credentials() {
    LoginResult result;
    result.ok = false;
    result.fault = LoginFault::InvalidCredentials;
    result.message = "That username or password is not recognised.";
    return result;
}

}  // namespace

LoginResult authenticate(const engine::Store& store, const engine::DeviceId& device,
                          std::string_view username, std::string_view password) {
    const std::string normalised = admin::normalise_username(username);
    const std::optional<admin::Person> person =
        admin::data::find_person_by_username(store, normalised);

    // A missing username fails exactly the same way a wrong password does,
    // below. See LoginFault::InvalidCredentials for why.
    if (!person) {
        return invalid_credentials();
    }

    // The password is checked before the disabled flag is, not after: a
    // disabled account with a leaked password should not let an attacker
    // learn the account exists by getting a different failure than a wrong
    // password would have produced against the same username. Argon2id
    // verification is also constant enough in shape that doing it first does
    // not create a timing tell for "this account is disabled" either.
    if (!platform::verify_password(password, person->password_hash)) {
        return invalid_credentials();
    }

    if (person->disabled) {
        LoginResult result;
        result.ok = false;
        result.fault = LoginFault::PersonDisabled;
        result.message = "This account has been disabled. Ask the owner to re-enable it.";
        return result;
    }

    engine::Session session;
    session.person = engine::record_id_from_string(person->id);
    session.device = device;
    session.display_name = person->display_name;
    session.is_owner = person->is_owner;
    session.rights = admin::data::rights_of(store, person->id);

    LoginResult result;
    result.ok = true;
    result.session = session;
    return result;
}

}  // namespace squiflow::app

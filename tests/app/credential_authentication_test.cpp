// authenticate(), driven the way the application drives it: a real
// Argon2id hash sitting in a real administration store, reached through the
// same registry-backed person_create/person_disable operations the rest of
// the suite uses. A rule that only holds when the hash is faked is not a
// rule.

#include "app/credential_authentication.hpp"

#include <memory>
#include <string>
#include <utility>

#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/administration/data/repository.hpp"
#include "modules/administration/module.hpp"
#include "modules/registry.hpp"
#include "platform/password_hash.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;

namespace admin = squiflow::modules::administration;
namespace app = squiflow::app;
namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace platform = squiflow::platform;
namespace protocol = squiflow::protocol;

namespace {

std::int64_t g_now = 1'700'000'000'000;
std::int64_t now() { return g_now += 1000; }

engine::Blob fields(std::initializer_list<std::pair<std::string, std::string>> pairs) {
    engine::Row row;
    for (const auto& pair : pairs) {
        row.set(pair.first, engine::Value::text(pair.second));
    }
    return engine::encode_payload(row);
}

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;

    Shop() {
        registry.add(admin::make_module(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(std::make_unique<engine::MemoryStore>(),
                                                       std::move(runner));
        database->open();
    }

    modules::Outcome run(protocol::OperationId operation, const std::string& record,
                         const engine::Blob& payload, const engine::Session& session) {
        modules::Call call;
        call.operation = operation;
        call.record_id = record;
        call.payload = payload;
        return registry.run(*database, call, session, engine::ConnectionState::Online);
    }

    template <typename Fn>
    auto look(Fn&& fn) const -> decltype(fn(std::declval<const engine::Store&>())) {
        using Result = decltype(fn(std::declval<const engine::Store&>()));
        Result result{};
        database->read([&](const engine::Store& store) { result = fn(store); });
        return result;
    }
};

engine::Session session_for(const std::string& person_id, std::uint64_t device,
                            bool is_owner, const engine::RightsSet& rights) {
    engine::Session session;
    session.person = engine::record_id_from_string(person_id);
    session.device = engine::RecordId{7, device};
    session.display_name = "whoever";
    session.is_owner = is_owner;
    session.rights = rights;
    return session;
}

engine::RightsSet everything() {
    engine::RightsSet rights;
    rights.grant_all();
    return rights;
}

const std::string kOwnerId = "00000000000000000000000000000001";
const std::string kStaffId = "00000000000000000000000000000002";
const engine::DeviceId kDevice{7, 42};

}  // namespace

int main() {
    section("a correct username and password sign in with the person's real rights");
    {
        Shop shop;
        const engine::Session installer = session_for(kOwnerId, 1, true, everything());
        const platform::PasswordHashResult hashed = platform::hash_password("correct horse battery staple");
        check(hashed.ok, "the owner's password hashed cleanly");

        const modules::Outcome created =
            shop.run(protocol::OperationId::person_create, kOwnerId,
                     fields({{"display_name", "Shopkeeper"},
                             {"username", "Shopkeeper"},
                             {"password_hash", hashed.hash}}),
                     installer);
        check(created.ok, "the owner was created");

        const app::LoginResult login = shop.look([&](const engine::Store& store) {
            return app::authenticate(store, kDevice, "ShopKeeper", "correct horse battery staple");
        });

        check(login.ok, "the sign-in succeeded");
        check(login.fault == app::LoginFault::None, "no fault is reported on success");
        check(login.session.is_signed_in(), "the returned session is signed in");
        check(login.session.display_name == "Shopkeeper", "the display name came from the person record");
        check(login.session.is_owner, "the owner's session is marked as owner");
        check(login.session.device == kDevice, "the device is carried through unchanged");
        check(login.session.rights == everything(),
              "the owner's session carries every right, same as the stored grants");
    }

    section("the username is matched the same normalised way it was stored");
    {
        Shop shop;
        const engine::Session installer = session_for(kOwnerId, 1, true, everything());
        const platform::PasswordHashResult hashed = platform::hash_password("another-strong-passphrase");

        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "  ShopKeeper  "},
                         {"password_hash", hashed.hash}}),
                 installer);

        const app::LoginResult login = shop.look([&](const engine::Store& store) {
            return app::authenticate(store, kDevice, "SHOPKEEPER", "another-strong-passphrase");
        });
        check(login.ok, "a differently-cased username still matches the normalised stored one");
    }

    section("a wrong password is refused without saying which half was wrong");
    {
        Shop shop;
        const engine::Session installer = session_for(kOwnerId, 1, true, everything());
        const platform::PasswordHashResult hashed = platform::hash_password("the-real-password");

        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "Shopkeeper"},
                         {"password_hash", hashed.hash}}),
                 installer);

        const app::LoginResult wrong_password = shop.look([&](const engine::Store& store) {
            return app::authenticate(store, kDevice, "Shopkeeper", "not-the-real-password");
        });
        check(!wrong_password.ok, "a wrong password is refused");
        check(wrong_password.fault == app::LoginFault::InvalidCredentials,
              "a wrong password reports InvalidCredentials");
        check(!wrong_password.session.is_signed_in(), "no session is returned on refusal");

        const app::LoginResult wrong_username = shop.look([&](const engine::Store& store) {
            return app::authenticate(store, kDevice, "NobodyByThatName", "the-real-password");
        });
        check(!wrong_username.ok, "an unknown username is refused");
        check(wrong_username.fault == app::LoginFault::InvalidCredentials,
              "an unknown username reports the identical fault a wrong password would");
        check(wrong_username.message == wrong_password.message,
              "the message text does not let the two failures be told apart");
    }

    section("a disabled account is refused with its own distinct fault");
    {
        Shop shop;
        const engine::Session installer = session_for(kOwnerId, 1, true, everything());
        const platform::PasswordHashResult owner_hashed = platform::hash_password("owner-passphrase");
        const platform::PasswordHashResult staff_hashed = platform::hash_password("staff-passphrase");

        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "Shopkeeper"},
                         {"password_hash", owner_hashed.hash}}),
                 installer);
        shop.run(protocol::OperationId::person_create, kStaffId,
                 fields({{"display_name", "Clerk"},
                         {"username", "Clerk"},
                         {"password_hash", staff_hashed.hash}}),
                 installer);
        const modules::Outcome disabled =
            shop.run(protocol::OperationId::person_disable, kStaffId, {}, installer);
        check(disabled.ok, "the owner switched the clerk off");

        const app::LoginResult login = shop.look([&](const engine::Store& store) {
            return app::authenticate(store, kDevice, "Clerk", "staff-passphrase");
        });
        check(!login.ok, "a disabled account with the right password still cannot sign in");
        check(login.fault == app::LoginFault::PersonDisabled,
              "the fault distinguishes a disabled account from wrong credentials");
        check(!login.session.is_signed_in(), "no session is returned for a disabled account");

        const app::LoginResult wrong_password_disabled = shop.look([&](const engine::Store& store) {
            return app::authenticate(store, kDevice, "Clerk", "not-the-staff-passphrase");
        });
        check(!wrong_password_disabled.ok, "a wrong password against a disabled account is still refused");
        check(wrong_password_disabled.fault == app::LoginFault::InvalidCredentials,
              "a wrong password is reported before the disabled flag is ever consulted");
    }

    section("hashing the same password twice never lets one hash unlock the other's account");
    {
        Shop shop;
        const engine::Session installer = session_for(kOwnerId, 1, true, everything());
        const platform::PasswordHashResult first = platform::hash_password("shared-passphrase");
        const platform::PasswordHashResult second = platform::hash_password("shared-passphrase");
        check(first.hash != second.hash, "the same password hashed twice produced different hashes");

        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "Shopkeeper"},
                         {"password_hash", first.hash}}),
                 installer);

        const app::LoginResult login = shop.look([&](const engine::Store& store) {
            return app::authenticate(store, kDevice, "Shopkeeper", "shared-passphrase");
        });
        check(login.ok, "the stored hash still verifies the same plaintext regardless of salt");
    }

    return squiflow::testing::report();
}

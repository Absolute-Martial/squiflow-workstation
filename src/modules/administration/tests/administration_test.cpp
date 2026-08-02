// Administration, driven the way the application drives it: through the
// registry, with a session, not by calling the service directly. A rule that
// only holds when the service is called by its own test is not a rule.

#include <memory>
#include <string>
#include <vector>

#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/administration/data/repository.hpp"
#include "modules/administration/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;

namespace admin = squiflow::modules::administration;
namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace protocol = squiflow::protocol;

namespace {

std::int64_t g_now = 1'700'000'000'000;
std::int64_t now() { return g_now += 1000; }

engine::RecordId id_of(std::uint64_t low) { return engine::RecordId{7, low}; }

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
                         const engine::Blob& payload, const engine::Session& session,
                         const std::string& key = {}) {
        modules::Call call;
        call.operation = operation;
        call.record_id = record;
        call.payload = payload;
        call.idempotency_key = key;
        return registry.run(*database, call, session, engine::ConnectionState::Online);
    }

    template <typename Fn>
    void look(Fn&& fn) const {
        database->read([&](const engine::Store& store) { fn(store); });
    }
};

engine::Session session_for(const std::string& person_id, std::uint64_t device,
                            bool is_owner, const engine::RightsSet& rights) {
    engine::Session session;
    session.person = engine::record_id_from_string(person_id);
    session.device = id_of(device);
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

// A record id has to be 32 hex characters, because that is what the engine
// accepts. These are the two people in this shop.
const std::string kOwnerId = "00000000000000000000000000000001";
const std::string kStaffId = "00000000000000000000000000000002";

}  // namespace

int main() {
    section("the first person created is the owner, and holds everything");
    {
        Shop shop;
        const engine::Session installer = session_for(kOwnerId, 1, true, everything());

        const modules::Outcome created =
            shop.run(protocol::OperationId::person_create, kOwnerId,
                     fields({{"display_name", "  Shopkeeper "},
                             {"username", "  ShopKeeper "},
                             {"password_hash", "hash"}}),
                     installer);
        check(created.ok, "the owner was created");
        check(!created.queued, "person_create is online-only, so nothing was queued");

        shop.look([&](const engine::Store& store) {
            const auto person = admin::data::find_person(store, kOwnerId);
            check(person.has_value(), "the person is there");
            check(person->is_owner, "and is the owner, without being asked to be");
            check(person->username == "shopkeeper", "the username was folded and trimmed");
            check(person->display_name == "Shopkeeper", "the name was trimmed");
            check(admin::data::rights_of(store, kOwnerId).count() == protocol::kRightCount,
                  "the owner holds every right");
        });

        const modules::Outcome second =
            shop.run(protocol::OperationId::person_create, kStaffId,
                     fields({{"display_name", "Helper"},
                             {"username", "helper"},
                             {"password_hash", "hash"}}),
                     installer);
        check(second.ok, "a second person can be added");
        shop.look([&](const engine::Store& store) {
            const auto person = admin::data::find_person(store, kStaffId);
            check(person.has_value() && !person->is_owner, "but is not an owner");
            check(admin::data::rights_of(store, kStaffId).empty(),
                  "and starts able to do nothing at all");
        });
    }

    section("a shop has one owner, and usernames are not shared");
    {
        Shop shop;
        const engine::Session installer = session_for(kOwnerId, 1, true, everything());
        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "shopkeeper"},
                         {"password_hash", "hash"}}),
                 installer);

        engine::Row asking_for_owner;
        asking_for_owner.set("display_name", engine::Value::text("Second"));
        asking_for_owner.set("username", engine::Value::text("second"));
        asking_for_owner.set("password_hash", engine::Value::text("hash"));
        asking_for_owner.set("is_owner", engine::Value::boolean(true));

        const modules::Outcome refused =
            shop.run(protocol::OperationId::person_create, kStaffId,
                     engine::encode_payload(asking_for_owner), installer);
        check(!refused.ok, "a second owner is refused");
        check(refused.reason == engine::DenialReason::None,
              "refused by the module, not by the permission rules");
        check(refused.error.find("already has an owner") != std::string::npos,
              "and says why in words");

        const modules::Outcome clash =
            shop.run(protocol::OperationId::person_create, kStaffId,
                     fields({{"display_name", "Second"},
                             {"username", "  SHOPKEEPER"},
                             {"password_hash", "hash"}}),
                     installer);
        check(!clash.ok, "a username differing only in case and spacing is refused");

        shop.look([&](const engine::Store& store) {
            check(!admin::data::find_person(store, kStaffId).has_value(),
                  "and neither refusal left half a person behind");
        });
    }

    section("a person with no name, no username or no password is refused");
    {
        Shop shop;
        const engine::Session installer = session_for(kOwnerId, 1, true, everything());

        check(!shop.run(protocol::OperationId::person_create, kOwnerId,
                        fields({{"username", "x"}, {"password_hash", "h"}}), installer)
                   .ok,
              "no name");
        check(!shop.run(protocol::OperationId::person_create, kOwnerId,
                        fields({{"display_name", "X"}, {"password_hash", "h"}}), installer)
                   .ok,
              "no username");
        check(!shop.run(protocol::OperationId::person_create, kOwnerId,
                        fields({{"display_name", "X"}, {"username", "x"}}), installer)
                   .ok,
              "no password hash");
        check(!shop.run(protocol::OperationId::person_create, kOwnerId,
                        fields({{"display_name", "X"},
                                {"username", "two words"},
                                {"password_hash", "h"}}),
                        installer)
                   .ok,
              "a username with a space in it");
        check(!shop.run(protocol::OperationId::person_create, {},
                        fields({{"display_name", "X"}, {"username", "x"}, {"password_hash", "h"}}),
                        installer)
                   .ok,
              "no record to save under");
    }

    section("nobody can give away a permission they do not hold");
    {
        Shop shop;
        const engine::Session owner = session_for(kOwnerId, 1, true, everything());
        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "shopkeeper"},
                         {"password_hash", "hash"}}),
                 owner);
        shop.run(protocol::OperationId::person_create, kStaffId,
                 fields({{"display_name", "Helper"},
                         {"username", "helper"},
                         {"password_hash", "hash"}}),
                 owner);

        const modules::Outcome granted =
            shop.run(protocol::OperationId::right_grant, kStaffId,
                     fields({{"right", "right_party_read"}}), owner);
        check(granted.ok, "the owner can give the helper a permission");

        // Somebody holding only the right to grant rights tries to hand out a
        // right they do not have themselves.
        engine::RightsSet only_granting;
        only_granting.grant(protocol::RightId::right_rights_grant);
        const engine::Session limited = session_for(kStaffId, 2, false, only_granting);

        const modules::Outcome escalation =
            shop.run(protocol::OperationId::right_grant, kStaffId,
                     fields({{"right", "right_person_manage"}}), limited);
        check(!escalation.ok, "refused");
        check(escalation.error.find("do not have yourself") != std::string::npos,
              "because the right to grant is not the right to grant everything");

        const modules::Outcome nonsense =
            shop.run(protocol::OperationId::right_grant, kStaffId,
                     fields({{"right", "right_to_be_taller"}}), owner);
        check(!nonsense.ok && nonsense.error.find("right_to_be_taller") != std::string::npos,
              "an unknown permission is named in the refusal");
    }

    section("the shop cannot be locked out of itself");
    {
        Shop shop;
        const engine::Session owner = session_for(kOwnerId, 1, true, everything());
        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "shopkeeper"},
                         {"password_hash", "hash"}}),
                 owner);
        shop.run(protocol::OperationId::person_create, kStaffId,
                 fields({{"display_name", "Helper"},
                         {"username", "helper"},
                         {"password_hash", "hash"}}),
                 owner);

        check(!shop.run(protocol::OperationId::person_disable, kOwnerId, {}, owner).ok,
              "the owner cannot be switched off");

        const engine::Session helper = session_for(kStaffId, 2, false, everything());
        check(!shop.run(protocol::OperationId::person_disable, kStaffId, {}, helper).ok,
              "and nobody can switch off their own sign-in");

        check(!shop.run(protocol::OperationId::right_revoke, kOwnerId,
                        fields({{"right", "right_rights_grant"}}), owner)
                   .ok,
              "the owner keeps every permission");

        const modules::Outcome switched_off =
            shop.run(protocol::OperationId::person_disable, kStaffId, {}, owner);
        check(switched_off.ok, "the owner can switch the helper off");
        check(shop.run(protocol::OperationId::person_disable, kStaffId, {}, owner).ok,
              "and asking twice is not an error");

        check(!shop.run(protocol::OperationId::right_grant, kStaffId,
                        fields({{"right", "right_party_read"}}), owner)
                   .ok,
              "a switched-off person is not given new permissions");
    }

    section("machines");
    {
        Shop shop;
        const engine::Session owner = session_for(kOwnerId, 1, true, everything());
        const std::string counter = engine::to_string(id_of(2));
        const std::string mine = engine::to_string(id_of(1));

        check(shop.run(protocol::OperationId::device_register, counter,
                       fields({{"name", "Counter machine"}}), owner)
                  .ok,
              "a machine is registered");
        check(!shop.run(protocol::OperationId::device_register, counter,
                        fields({{"name", "Counter machine"}}), owner)
                   .ok,
              "registering it twice is refused");
        check(!shop.run(protocol::OperationId::device_register,
                        engine::to_string(id_of(3)), fields({{"name", "   "}}), owner)
                   .ok,
              "a machine with no name is refused");

        shop.run(protocol::OperationId::device_register, mine, fields({{"name", "Back office"}}),
                 owner);
        check(!shop.run(protocol::OperationId::device_retire, mine, {}, owner).ok,
              "you cannot retire the machine you are sitting at");
        check(shop.run(protocol::OperationId::device_retire, counter, {}, owner).ok,
              "but you can retire the other one");

        shop.look([&](const engine::Store& store) {
            const auto device = admin::data::find_device(store, counter);
            check(device.has_value() && device->retired, "and it is recorded as retired");
        });
    }

    section("settings are sent to the other machine, and people are not");
    {
        Shop shop;
        const engine::Session owner = session_for(kOwnerId, 1, true, everything());

        const modules::Outcome saved =
            shop.run(protocol::OperationId::shop_setting_update, "shop.name",
                     fields({{"value", "Ram Printing Press"}}), owner, "key-setting-1");
        check(saved.ok, "the setting was saved");
        check(saved.queued, "and queued, because both machines need it");

        shop.look([&](const engine::Store& store) {
            check(admin::data::get_setting(store, "shop.name").value_or(std::string{}) ==
                      "Ram Printing Press",
                  "the value is there");
        });

        const modules::Outcome empty =
            shop.run(protocol::OperationId::shop_setting_update, "shop.address", {}, owner,
                     "key-setting-2");
        check(!empty.ok, "a setting with nothing to set is refused");
    }

    section("switching parts of the application off");
    {
        Shop shop;
        const engine::Session owner = session_for(kOwnerId, 1, true, everything());

        check(shop.run(protocol::OperationId::module_activation_set, "activation",
                       fields({{"disabled", "files, companion"}}), owner)
                  .ok,
              "two extra parts can be switched off at once");

        shop.look([&](const engine::Store& store) {
            check(admin::data::disabled_modules(store).size() == 2, "both were recorded");
        });

        check(!shop.run(protocol::OperationId::module_activation_set, "activation",
                        fields({{"disabled", "parties"}}), owner)
                   .ok,
              "a core part cannot be switched off");

        check(!shop.run(protocol::OperationId::module_activation_set, "activation",
                        fields({{"disabled", "astrology"}}), owner)
                   .ok,
              "and neither can something that does not exist");

        check(shop.run(protocol::OperationId::module_activation_set, "activation",
                       fields({{"disabled", ""}}), owner)
                  .ok,
              "everything can be switched back on");
        shop.look([&](const engine::Store& store) {
            check(admin::data::disabled_modules(store).empty(), "and the table is empty again");
        });
    }

    section("every administrative act is in the log, and the log can be read");
    {
        Shop shop;
        const engine::Session owner = session_for(kOwnerId, 1, true, everything());
        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "shopkeeper"},
                         {"password_hash", "hash"}}),
                 owner);
        shop.run(protocol::OperationId::person_create, kStaffId,
                 fields({{"display_name", "Helper"},
                         {"username", "helper"},
                         {"password_hash", "hash"}}),
                 owner);
        shop.run(protocol::OperationId::right_grant, kStaffId,
                 fields({{"right", "right_party_read"}}), owner);

        const modules::Outcome exported =
            shop.run(protocol::OperationId::audit_export, {}, {}, owner);
        check(exported.ok, "the log can be exported");
        check(exported.rows.size() == 3, "three acts, three entries");
        check(exported.rows.front().get("operation").text_or({}) == "person_create",
              "in the order they happened");
        check(exported.rows.front().get("person").text_or({}) == kOwnerId,
              "recording who did it");
        check(!exported.rows.back().get("summary").text_or({}).empty(),
              "and what they did, in words");

        engine::Row limited;
        limited.set("limit", engine::Value::integer(1));
        const modules::Outcome one =
            shop.run(protocol::OperationId::audit_export, {}, engine::encode_payload(limited),
                     owner);
        check(one.rows.size() == 1, "a limit is honoured");

        engine::Row bad;
        bad.set("limit", engine::Value::integer(-5));
        const modules::Outcome nonsense =
            shop.run(protocol::OperationId::audit_export, {}, engine::encode_payload(bad), owner);
        check(!nonsense.ok, "and a negative one is refused rather than wrapped around");
    }

    section("a refused change leaves nothing behind");
    {
        Shop shop;
        const engine::Session owner = session_for(kOwnerId, 1, true, everything());
        shop.run(protocol::OperationId::person_create, kOwnerId,
                 fields({{"display_name", "Shopkeeper"},
                         {"username", "shopkeeper"},
                         {"password_hash", "hash"}}),
                 owner);

        const modules::Outcome refused =
            shop.run(protocol::OperationId::person_create, kStaffId,
                     fields({{"display_name", "Helper"},
                             {"username", "shopkeeper"},
                             {"password_hash", "hash"}}),
                     owner);
        check(!refused.ok, "the duplicate username was refused");

        const modules::Outcome log = shop.run(protocol::OperationId::audit_export, {}, {}, owner);
        check(log.rows.size() == 1,
              "and the refused attempt did not write a log entry either - the whole "
              "transaction went back");
    }

    section("a damaged request is refused in words");
    {
        Shop shop;
        const engine::Session owner = session_for(kOwnerId, 1, true, everything());
        const engine::Blob rubbish{'n', 'o', 't', ' ', 'a', ' ', 'p', 'a', 'y', 'l', 'o', 'a', 'd'};
        const modules::Outcome outcome =
            shop.run(protocol::OperationId::person_create, kOwnerId, rubbish, owner);
        check(!outcome.ok, "refused");
        check(outcome.error.find("could not be read") != std::string::npos,
              "and not as a crash");
    }

    return squiflow::testing::report();
}

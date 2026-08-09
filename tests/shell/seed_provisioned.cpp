// First-run provisions a workstation store for the authenticated-startup e2e
// test: owner person, the counter machine, and every right the owner holds.
// This is the provisioning the real first-run flow will perform; the shell
// binary is only ever launched against a store this tool or the real flow
// produced.

#if defined(SQUIFLOW_WITH_SQLITE)

#include "app/composition_root.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/migration_runner.hpp"
#include "engine/storage/sqlite_store.hpp"
#include "modules/administration/data/repository.hpp"
#include "modules/administration/domain/device.hpp"
#include "modules/administration/domain/person.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

const std::string kPerson = "c2000000000000000000000000000001";
const std::string kDevice = "c2000000000000000000000000000002";

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: seed_provisioned <database-path>\n";
        return 2;
    }
    const std::string database_path = argv[1];
    std::filesystem::create_directories(
        std::filesystem::path(database_path).parent_path());

    auto store = std::make_unique<squiflow::engine::SqliteStore>(database_path);
    squiflow::engine::MigrationRunner runner{[] { return std::int64_t{42}; }};
    squiflow::modules::Registry registry{[] { return std::int64_t{42}; }};
    squiflow::app::register_all_modules(registry,
                                        [] { return std::int64_t{42}; });
    registry.collect_migrations(runner);

    try {
        squiflow::engine::Database database(std::move(store), std::move(runner));
        database.open();
        database.write([&](squiflow::engine::Transaction& transaction) {
            squiflow::modules::administration::Person person;
            person.id = kPerson;
            person.display_name = "First Owner";
            person.username = "owner";
            person.password_hash = "provisioned-through-the-store, never typed";
            person.is_owner = true;
            squiflow::modules::administration::data::save_person(transaction,
                                                                 person);
            squiflow::modules::administration::Device machine;
            machine.id = kDevice;
            machine.name = "counter";
            squiflow::modules::administration::data::save_device(transaction,
                                                                 machine);
        });
    } catch (const std::exception& error) {
        std::cerr << "provisioning failed: " << error.what() << "\n";
        return 1;
    }
    return 0;
}

#else
int main() { return 0; }
#endif

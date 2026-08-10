# The engine: mechanisms every module borrows. A mechanism owns no business
# entity, which is exactly what separates it from a module.

add_library(squiflow_engine STATIC
    audit/audit_log.cpp
    records/money.cpp
    records/quantity.cpp
    records/identity.cpp
    records/lifecycle.cpp
    records/numbering.cpp
    records/approval.cpp
    records/payload.cpp
    identity/rights_set.cpp
    identity/capability.cpp
    storage/store.cpp
    storage/memory_store.cpp
    storage/migration_runner.cpp
    storage/writer.cpp
    storage/database.cpp
    sync/outbox.cpp
    sync/cursor.cpp
    sync/conflict.cpp)

add_library(squiflow::engine ALIAS squiflow_engine)

# Not CMAKE_SOURCE_DIR: that is the top of whoever is building, which is not
# necessarily us. This target must describe itself.
target_include_directories(squiflow_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# The SQLite backing is the only part of the engine that needs a library from
# outside. It is switchable so a machine without SQLite can still build and run
# everything else - the verification lane in tools/sandbox uses this. A real
# build never switches it off, and CI asserts it is on.
option(SQUIFLOW_WITH_SQLITE "Build the SQLite backing of the store" ON)

if(SQUIFLOW_WITH_SQLITE)
    find_package(SQLite3 REQUIRED)
    target_sources(squiflow_engine PRIVATE
        storage/sqlite_statement.cpp
        storage/sqlite_store.cpp)
    target_link_libraries(squiflow_engine PRIVATE SQLite3::SQLite3)
    target_compile_definitions(squiflow_engine PUBLIC SQUIFLOW_WITH_SQLITE=1)

    # 3.51.3 fixed a WAL reset corruption bug. This application uses WAL on
    # every machine, so that version is a floor, not a preference.
    if(SQLite3_VERSION VERSION_LESS "3.51.3")
        message(FATAL_ERROR
            "SQLite ${SQLite3_VERSION} is too old: 3.51.3 fixed a WAL reset "
            "corruption bug and this application always runs in WAL mode")
    endif()
else()
    message(WARNING
        "SQUIFLOW_WITH_SQLITE=OFF: building without the SQLite store. "
        "This is a verification lane, not a shippable build.")
endif()

find_package(Threads REQUIRED)

target_link_libraries(squiflow_engine
    PUBLIC  squiflow::protocol Threads::Threads
    PRIVATE squiflow::warnings squiflow::hardening squiflow::sanitizers)

if(SQUIFLOW_USE_PCH AND SQUIFLOW_COMMON_PCH_HEADERS)
    target_precompile_headers(squiflow_engine PRIVATE ${SQUIFLOW_COMMON_PCH_HEADERS})
endif()

set_target_properties(squiflow_engine PROPERTIES FOLDER "engine")

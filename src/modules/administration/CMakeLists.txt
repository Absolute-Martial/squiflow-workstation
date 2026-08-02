# Administration: people, permissions, machines, settings, activation, audit.
#
# Core, and required by nothing - it is the module everything else assumes
# rather than links against. Rights and operations reach other modules through
# the protocol enums, never through this target.

squiflow_add_module(administration
    TIER core
    SOURCES
        module.cpp
        domain/person.cpp
        domain/device.cpp
        data/tables.cpp
        data/repository.cpp
        service/administration_service.cpp
    TEST_SOURCES
        tests/administration_test.cpp)

# squiflow_add_module puts src/modules on the include path so a module can
# include its own headers. Headers here are written as
# "modules/administration/...", the same spelling every other layer uses, so
# the directory above that is needed too. One spelling everywhere is worth one
# extra line here.
target_include_directories(squiflow_module_administration
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(TARGET squiflow_module_administration_test)
    target_include_directories(squiflow_module_administration_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

target_link_libraries(squiflow_module_administration
    PUBLIC squiflow::modules::framework)

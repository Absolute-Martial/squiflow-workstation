squiflow_add_module(parties
    TIER core
    SOURCES
        module.cpp
        domain/party.cpp
        data/tables.cpp
        data/repository.cpp
        service/parties_service.cpp
    TEST_SOURCES
        tests/parties_test.cpp)

target_include_directories(squiflow_module_parties
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(TARGET squiflow_module_parties_test)
    target_include_directories(squiflow_module_parties_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

target_link_libraries(squiflow_module_parties
    PUBLIC squiflow::modules::framework)

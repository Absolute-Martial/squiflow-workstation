squiflow_add_module(sourcing
    TIER extra
    REQUIRES parties
    SOURCES module.cpp domain/sourcing.cpp data/tables.cpp data/repository.cpp service/sourcing_service.cpp
    TEST_SOURCES tests/sourcing_test.cpp)
target_include_directories(squiflow_module_sourcing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
if(TARGET squiflow_module_sourcing_test)
    target_include_directories(squiflow_module_sourcing_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()
target_link_libraries(squiflow_module_sourcing PUBLIC squiflow::modules::framework)

squiflow_add_module(agreements
    TIER extra
    REQUIRES parties pricing
    SOURCES module.cpp domain/agreement.cpp domain/consumption.cpp data/tables.cpp data/repository.cpp service/agreements_service.cpp
    TEST_SOURCES tests/agreements_test.cpp)
target_include_directories(squiflow_module_agreements PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
if(TARGET squiflow_module_agreements_test)
    target_include_directories(squiflow_module_agreements_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()
target_link_libraries(squiflow_module_agreements PUBLIC squiflow::modules::framework)

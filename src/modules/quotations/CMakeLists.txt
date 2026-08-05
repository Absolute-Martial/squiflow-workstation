squiflow_add_module(quotations
    TIER extra
    REQUIRES catalog pricing parties
    SOURCES
        module.cpp
        domain/quotation.cpp
        data/tables.cpp
        data/repository.cpp
        service/quotations_service.cpp
    TEST_SOURCES
        tests/quotations_test.cpp)

target_include_directories(squiflow_module_quotations
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(TARGET squiflow_module_quotations_test)
    target_include_directories(squiflow_module_quotations_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

target_link_libraries(squiflow_module_quotations
    PUBLIC squiflow::modules::framework)

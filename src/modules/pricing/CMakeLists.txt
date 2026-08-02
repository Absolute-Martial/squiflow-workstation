squiflow_add_module(pricing
    TIER core
    SOURCES
        module.cpp
        domain/rate.cpp
        data/tables.cpp
        data/repository.cpp
        service/pricing_service.cpp
    TEST_SOURCES
        tests/pricing_test.cpp)

target_include_directories(squiflow_module_pricing
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(TARGET squiflow_module_pricing_test)
    target_include_directories(squiflow_module_pricing_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

target_link_libraries(squiflow_module_pricing
    PUBLIC squiflow::modules::framework)

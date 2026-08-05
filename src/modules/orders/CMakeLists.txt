squiflow_add_module(orders
    TIER core
    REQUIRES catalog pricing parties
    SOURCES
        module.cpp
        domain/order.cpp
        data/tables.cpp
        data/repository.cpp
        service/orders_service.cpp
    TEST_SOURCES
        tests/orders_test.cpp)

target_include_directories(squiflow_module_orders
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(TARGET squiflow_module_orders_test)
    target_include_directories(squiflow_module_orders_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

target_link_libraries(squiflow_module_orders
    PUBLIC squiflow::modules::framework)

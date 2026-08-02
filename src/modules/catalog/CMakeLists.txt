squiflow_add_module(catalog
    TIER core
    SOURCES
        module.cpp
        domain/product.cpp
        data/tables.cpp
        data/repository.cpp
        service/catalog_service.cpp
    TEST_SOURCES
        tests/catalog_test.cpp)

target_include_directories(squiflow_module_catalog
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(TARGET squiflow_module_catalog_test)
    target_include_directories(squiflow_module_catalog_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()

target_link_libraries(squiflow_module_catalog
    PUBLIC squiflow::modules::framework)

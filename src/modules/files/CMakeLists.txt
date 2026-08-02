squiflow_add_module(files
    TIER extra
    SOURCES module.cpp domain/file_asset.cpp data/tables.cpp data/repository.cpp service/files_service.cpp
    TEST_SOURCES tests/files_test.cpp)
target_include_directories(squiflow_module_files PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
if(TARGET squiflow_module_files_test)
    target_include_directories(squiflow_module_files_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
endif()
target_link_libraries(squiflow_module_files PUBLIC squiflow::modules::framework)

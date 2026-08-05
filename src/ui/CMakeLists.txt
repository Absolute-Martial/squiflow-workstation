if(SQUIFLOW_WITH_QT)
    target_compile_definitions(squiflow_shell PRIVATE SQUIFLOW_WITH_QT=1)
    set_target_properties(squiflow_shell PROPERTIES AUTOMOC ON)
    target_sources(squiflow_shell PRIVATE
        ../shell/paged_list_model_qt.cpp
        ../shell/list_screen_bridge_qt.cpp
        ../shell/navigation_model_qt.cpp
        ../shell/navigation_bridge_qt.cpp
        ../shell/qml_surface_qt.cpp)
    target_link_libraries(squiflow_shell PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
    target_link_libraries(squiflow_workstation PRIVATE squiflow::shell)

    if(SQUIFLOW_BUILD_TESTS)
        add_executable(qt_bridge_test ../../tests/shell/qt_bridge_test.cpp)
        target_compile_definitions(qt_bridge_test PRIVATE SQUIFLOW_WITH_QT=1)
        target_link_libraries(qt_bridge_test PRIVATE squiflow::shell squiflow::testing Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick squiflow::warnings squiflow::hardening squiflow::sanitizers)
        add_test(NAME shell.qt_bridge COMMAND qt_bridge_test)
    endif()
endif()

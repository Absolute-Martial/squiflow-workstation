if(SQUIFLOW_WITH_QT)
    target_compile_definitions(squiflow_shell PRIVATE SQUIFLOW_WITH_QT=1)
    set_target_properties(squiflow_shell PROPERTIES AUTOMOC ON)

    if(SQUIFLOW_WITH_UI_FLUENT)
        # Hybrid Fluent UI sourcing strategy: docs/plan/phase-7-fluent-ui-sourcing.md
        # Layer 2/3 are pure-QML modules; they only need an import path, not a
        # compiled target, so they are wired as string literals here rather
        # than add_subdirectory.
        target_compile_definitions(squiflow_shell PRIVATE
            SQUIFLOW_WITH_UI_FLUENT=1
            "SQUIFLOW_UI_FLUENTCONTROLS_IMPORT_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../../external/ui-fluent/fluentcontrols\""
            "SQUIFLOW_UI_RINUI_IMPORT_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../../external/ui-fluent/rin-ui/RinUI\"")

        # zhuzichu520/FluentUI is Layer 2's C++/QML half (Flu-prefixed types,
        # e.g. FluButton, so it cannot collide with QQC2 or the other layers).
        # Not yet enabled by default: it additionally requires the Qt Widgets
        # and PrintSupport modules on top of the Core/Gui/Qml/Quick this shell
        # already links, which is a real cost worth taking on deliberately
        # rather than silently. Uncomment once that trade-off is confirmed on
        # a real Qt machine:
        # set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        # add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/ui-fluent/fluentui
        #     ${CMAKE_CURRENT_BINARY_DIR}/ui-fluent-fluentui)
        # target_link_libraries(squiflow_shell PRIVATE fluentuiplugin)
    endif()
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

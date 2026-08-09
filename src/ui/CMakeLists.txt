if(SQUIFLOW_WITH_QT)
    find_package(Qt6 6.11.1 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

    target_compile_definitions(squiflow_shell PRIVATE SQUIFLOW_WITH_QT=1)
    set_target_properties(squiflow_shell PROPERTIES AUTOMOC ON)

    # The workstation executable is declared here, in the directory that owns
    # the module files: the Qt tooling (qmlcachegen, autogen, metatypes)
    # derives every output path from the module's directory. Declaring the
    # module from a sibling directory would make it emit "<target>_../ui/..."
    # paths that the Ninja generator's mkdir cannot create, so target and
    # module share this directory by design.
    add_executable(squiflow_workstation ../app/main.cpp)
    target_link_libraries(squiflow_workstation PRIVATE squiflow::app)
    target_compile_definitions(squiflow_workstation PRIVATE SQUIFLOW_WITH_QT=1)
    set_target_properties(squiflow_workstation PROPERTIES AUTOMOC ON)

    # The workstation QML module is declared here, in the directory that owns
    # the module files. qt_add_qml_module derives every qmlcache output from
    # each file's path relative to this directory: a module declared from a
    # sibling directory produces "<target>_../ui/..." cache paths, which the
    # Ninja generator's mkdir cannot create (".." with missing parents).
    qt_add_qml_module(squiflow_workstation URI SquiFlow VERSION 1.0
        QML_FILES
            Main.qml
            Theme.qml
            common/DataList.qml
            common/Form.qml
            common/FilePreview.qml
            common/PageScaffold.qml
            common/CommandBar.qml
            common/StatusBanner.qml
            common/MetricCard.qml
            common/SectionCard.qml
            common/EmptyState.qml
            common/ErrorState.qml
            common/LoadingSkeleton.qml
            common/ConfirmDialog.qml
            common/UnsavedChangesDialog.qml
            common/PrimaryListPage.qml
            common/OperationalModulePage.qml
            common/RecordDetailPane.qml
            common/RecordActionDialog.qml
            dashboard/DashboardPage.qml
            parties/PartiesPage.qml
            parties/PartyDetailPage.qml
            parties/PartyFormPage.qml
            parties/ContactEditor.qml
            catalog/CatalogPage.qml
            catalog/ProductDetailPage.qml
            catalog/ProductFormPage.qml
            pricing/PricingPage.qml
            pricing/RateDetailPage.qml
            pricing/RateFormPage.qml
            pricing/PriceResolutionPanel.qml
            orders/OrdersPage.qml
            orders/OrderDetailPage.qml
            orders/OrderEditorPage.qml
            orders/OrderLineEditor.qml
            orders/PriceEvidenceBadge.qml
            counter/CounterSalePage.qml
            counter/CartLine.qml
            counter/PaymentPanel.qml
            receivables/ReceivablesPage.qml
            receivables/InvoiceDetailPage.qml
            receivables/PaymentAllocationPage.qml
            receivables/StatementPanel.qml
            receivables/DocumentDeliveryPanel.qml
            quotations/QuotationsPage.qml
            agreements/AgreementsPage.qml
            jobs/JobsPage.qml
            sourcing/SourcingPage.qml
            companion/CompanionPage.qml
            files/FilesPage.qml
            administration/AdministrationPage.qml
            navigation/NavigationRail.qml
            navigation/NavigationDrawer.qml
            navigation/NavigationHost.qml
            navigation/NoAccessibleModules.qml
            screens/ModuleListScreen.qml
        RESOURCES
            documents/quotation.html
            documents/invoice.html
            documents/statement.html
        RESOURCE_PREFIX /qt/qml)

    qt_add_resources(squiflow_workstation squiflow_ui_config
        PREFIX "/" FILES qtquickcontrols2.conf)

    target_link_libraries(squiflow_workstation PRIVATE
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)

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
    target_sources(squiflow_workstation PRIVATE workstation_main_qt.cpp
        real_startup_services_qt.cpp)

    target_sources(squiflow_shell PRIVATE
        ../shell/paged_list_model_qt.cpp
        ../shell/list_screen_bridge_qt.cpp
        ../shell/record_screen_bridge_qt.cpp
        ../shell/navigation_model_qt.cpp
        ../shell/navigation_bridge_qt.cpp
        ../shell/qml_surface_qt.cpp
        ../shell/document_render_qt.cpp
        ../shell/image_provider_qt.cpp
        ../shell/dashboard_model_qt.cpp
        ../shell/dashboard_bridge_qt.cpp
        ../shell/shell_state_qt.cpp
        ../shell/native_window_bridge_qt.cpp)
    target_link_libraries(squiflow_shell PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

    if(SQUIFLOW_WITH_QWINDOWKIT)
        set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "" FORCE)
        set(QWINDOWKIT_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
        set(QWINDOWKIT_BUILD_QUICK ON CACHE BOOL "" FORCE)
        set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF CACHE BOOL "" FORCE)
        set(QWINDOWKIT_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/../../external/ui-fluent/qwindowkit"
            "${CMAKE_CURRENT_BINARY_DIR}/qwindowkit"
            EXCLUDE_FROM_ALL)
        target_link_libraries(squiflow_shell PRIVATE QWKQuick)
        target_compile_definitions(squiflow_shell PRIVATE
            SQUIFLOW_WITH_QWINDOWKIT=1)
    endif()
    target_link_libraries(squiflow_workstation PRIVATE squiflow::shell squiflow::engine)

    if(SQUIFLOW_BUILD_TESTS)
        add_executable(qt_bridge_test ../../tests/shell/qt_bridge_test.cpp)
        target_compile_definitions(qt_bridge_test PRIVATE SQUIFLOW_WITH_QT=1)
        target_link_libraries(qt_bridge_test PRIVATE squiflow::shell squiflow::testing Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick squiflow::warnings squiflow::hardening squiflow::sanitizers)
        add_test(NAME shell.qt_bridge COMMAND qt_bridge_test)

        if(SQUIFLOW_WITH_SQLITE)
            # The authenticated-startup journey against a real SQLite store and
            # the real shipped binary. Memory lanes silently report a pass at
            # build time (see the test source): they cannot pretend a record
            # ever survived a process exit.
            add_executable(seed_provisioned ../../tests/shell/seed_provisioned.cpp)
            target_link_libraries(seed_provisioned PRIVATE squiflow::app squiflow::engine squiflow::testing squiflow::warnings squiflow::hardening squiflow::sanitizers)
            add_test(NAME app.authenticated_startup
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/shell/e2e_authenticated_startup.sh
                        $<TARGET_FILE:squiflow_workstation>
                        $<TARGET_FILE:seed_provisioned>)
        endif()
    endif()
endif()
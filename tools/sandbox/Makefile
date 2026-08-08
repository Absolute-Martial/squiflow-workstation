# Verification lane, not the build.
#
# The real build is CMake, Qt and the Windows compiler. This makefile exists so
# the parts that depend on neither Qt nor SQLite can be compiled and executed
# anywhere, including on a machine with nothing installed beyond a compiler.
#
# The language is C++23, the same as the real build. Note that a compiler can
# accept -std=c++23 while shipping only part of the C++23 *library*; the
# integrity pass refuses the library headers that are not available here, so
# the two lanes cannot silently diverge.
#
#   make -f tools/sandbox/Makefile check      everything
#   make -f tools/sandbox/Makefile tests      compile and run the test programs
#   make -f tools/sandbox/Makefile headers    every header compiles alone
#   make -f tools/sandbox/Makefile integrity  per-file checks

.RECIPEPREFIX := >
.DEFAULT_GOAL := check

CXX ?= g++
STD ?= c++23
BUILD ?= build/sandbox
PYTHON ?= python3

SODIUM_PREFIX ?= build/vendor/libsodium
SODIUM_LIB := $(SODIUM_PREFIX)/lib/libsodium.a

INCLUDES := -Iexternal/protocol/include -isystem external/spdlog/include -isystem external/crashcatch/include -isystem $(SODIUM_PREFIX)/include -Isrc -Itests

WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
            -Wold-style-cast -Wnon-virtual-dtor -Wcast-qual -Wdouble-promotion \
            -Wformat=2 -Wimplicit-fallthrough -Wextra-semi -Wnull-dereference \
            -Wno-error=null-dereference \
            -Woverloaded-virtual -Wunused -Werror

# -pthread from here on: the writer gate is genuinely concurrent, and a test
# of a queue that never starts a thread proves nothing.
CXXFLAGS := -std=$(STD) -O1 -g -pthread $(WARNINGS) $(INCLUDES)

PROTOCOL_SRC := external/protocol/src/module_graph.cpp \
                external/protocol/src/operation_table.cpp \
                external/protocol/src/workflow_table.cpp

PLATFORM_SRC := src/platform/paths.cpp \
                src/platform/local_directory_probe.cpp \
                src/platform/path_environment_posix.cpp

# The log lane is a separate list: nothing above the platform boundary should
# have to link path resolution in order to write a log line.
LOGGING_SRC := src/platform/log_record.cpp \
               src/platform/log_clock.cpp \
               src/platform/log_level_policy.cpp \
               src/platform/log_throttle.cpp \
               src/platform/log_backtrace.cpp \
               src/platform/async_log_sink.cpp \
               src/platform/log_formatter.cpp \
               src/platform/local_log_storage.cpp \
               src/platform/rotating_log_file.cpp \
               src/platform/logger.cpp \
               src/platform/crash_breadcrumb.cpp

CRASH_SRC := src/platform/crash_handler_posix.cpp

SINGLE_INSTANCE_SRC := src/platform/single_instance_name.cpp \
                       src/platform/single_instance_posix.cpp

SECRETS_SRC := src/platform/secret_buffer.cpp \
               src/platform/secret_identity.cpp \
               src/platform/secret_envelope.cpp

NETWORK_STATE_SRC := src/platform/network_state.cpp \
                     src/platform/network_monitor.cpp

BACKGROUND_SERVICES_SRC := src/platform/background_service.cpp \
                           src/platform/background_executor.cpp \
                           src/platform/background_supervisor.cpp

APP_STARTUP_SRC := src/app/startup.cpp \
                   src/app/startup_sequence.cpp \
                   src/app/activation_controller.cpp \
                   src/app/qt_message_mapping.cpp \
                   src/platform/log_record.cpp

ENGINE_SRC := src/engine/records/identity.cpp \
              src/engine/records/quantity.cpp \
              src/engine/records/money.cpp \
              src/engine/records/lifecycle.cpp \
              src/engine/records/numbering.cpp \
              src/engine/records/approval.cpp \
              src/engine/identity/rights_set.cpp \
              src/engine/identity/capability.cpp

# The payload codec lives among the records but is built with storage: it turns
# a Row into bytes, so it needs the row. Listing it with the pure records made
# the domain test link against a store it does not use.
PAYLOAD_SRC := src/engine/records/payload.cpp

AUDIT_SRC := src/engine/audit/audit_log.cpp

STORAGE_SRC := src/engine/storage/store.cpp \
               src/engine/storage/memory_store.cpp \
               src/engine/storage/migration_runner.cpp \
               src/engine/storage/writer.cpp \
               src/engine/storage/database.cpp

SYNC_SRC := src/engine/sync/outbox.cpp \
            src/engine/sync/cursor.cpp \
            src/engine/sync/conflict.cpp

MODULES_SRC := src/modules/registry.cpp $(AUDIT_SRC)

QUOTE_TO_ORDER_SRC := src/workflows/quote_to_order.cpp
ORDER_TO_JOBS_SRC := src/workflows/order_to_jobs.cpp
ISSUE_INVOICE_SRC := src/workflows/issue_invoice.cpp
CANCEL_REISSUE_SRC := src/workflows/cancel_and_reissue.cpp
TAKE_PAYMENT_SRC := src/workflows/take_payment.cpp
COUNTER_SALE_SRC := src/workflows/counter_sale.cpp
APPLY_AGREEMENT_SRC := src/workflows/apply_agreement.cpp
RECORD_PURCHASE_SRC := src/workflows/record_purchase.cpp
DOCUMENT_DELIVERY_SRC := src/workflows/document_delivery.cpp
WORKFLOW_REGISTRATION_SRC := src/workflows/registration.cpp
ALL_WORKFLOWS_SRC := $(QUOTE_TO_ORDER_SRC) $(ORDER_TO_JOBS_SRC) \
                     $(ISSUE_INVOICE_SRC) $(CANCEL_REISSUE_SRC) \
                     $(TAKE_PAYMENT_SRC) $(COUNTER_SALE_SRC) \
                     $(APPLY_AGREEMENT_SRC) $(RECORD_PURCHASE_SRC) \
                     $(DOCUMENT_DELIVERY_SRC) $(WORKFLOW_REGISTRATION_SRC)

CATALOG_SRC := src/modules/catalog/module.cpp \
               src/modules/catalog/domain/product.cpp \
               src/modules/catalog/data/tables.cpp \
               src/modules/catalog/data/repository.cpp \
               src/modules/catalog/service/catalog_service.cpp

PRICING_SRC := src/modules/pricing/module.cpp \
               src/modules/pricing/domain/rate.cpp \
               src/modules/pricing/data/tables.cpp \
               src/modules/pricing/data/repository.cpp \
               src/modules/pricing/service/pricing_service.cpp

ORDERS_SRC := src/modules/orders/module.cpp \
              src/modules/orders/domain/order.cpp \
              src/modules/orders/data/tables.cpp \
              src/modules/orders/data/repository.cpp \
              src/modules/orders/service/orders_service.cpp

RECEIVABLES_SRC := src/modules/receivables/module.cpp \
                   src/modules/receivables/domain/invoice.cpp \
                   src/modules/receivables/domain/payment.cpp \
                   src/modules/receivables/domain/credit_account.cpp \
                   src/modules/receivables/domain/document_number_block.cpp \
                   src/modules/receivables/domain/document_delivery.cpp \
                   src/modules/receivables/domain/statement.cpp \
                   src/modules/receivables/data/tables.cpp \
                   src/modules/receivables/data/repository.cpp \
                   src/modules/receivables/service/receivables_service.cpp

JOBS_SRC := src/modules/jobs/module.cpp \
            src/modules/jobs/domain/job.cpp \
            src/modules/jobs/data/tables.cpp \
            src/modules/jobs/data/repository.cpp \
            src/modules/jobs/service/jobs_service.cpp

QUOTATIONS_SRC := src/modules/quotations/module.cpp \
                  src/modules/quotations/domain/quotation.cpp \
                  src/modules/quotations/data/tables.cpp \
                  src/modules/quotations/data/repository.cpp \
                  src/modules/quotations/service/quotations_service.cpp

AGREEMENTS_SRC := src/modules/agreements/module.cpp \
                  src/modules/agreements/domain/agreement.cpp \
                  src/modules/agreements/domain/consumption.cpp \
                  src/modules/agreements/data/tables.cpp \
                  src/modules/agreements/data/repository.cpp \
                  src/modules/agreements/service/agreements_service.cpp

SOURCING_SRC := src/modules/sourcing/module.cpp \
                src/modules/sourcing/domain/sourcing.cpp \
                src/modules/sourcing/data/tables.cpp \
                src/modules/sourcing/data/repository.cpp \
                src/modules/sourcing/service/sourcing_service.cpp

COMPANION_SRC := src/modules/companion/module.cpp \
                 src/modules/companion/domain/task.cpp \
                 src/modules/companion/data/tables.cpp \
                 src/modules/companion/data/repository.cpp \
                 src/modules/companion/service/companion_service.cpp

FILES_SRC := src/modules/files/module.cpp \
             src/modules/files/domain/file_asset.cpp \
             src/modules/files/data/tables.cpp \
             src/modules/files/data/repository.cpp \
             src/modules/files/service/files_service.cpp

PARTIES_SRC := src/modules/parties/module.cpp \
               src/modules/parties/domain/party.cpp \
               src/modules/parties/data/tables.cpp \
               src/modules/parties/data/repository.cpp \
               src/modules/parties/service/parties_service.cpp

ADMIN_SRC := src/modules/administration/module.cpp \
             src/modules/administration/domain/person.cpp \
             src/modules/administration/domain/device.cpp \
             src/modules/administration/data/tables.cpp \
             src/modules/administration/data/repository.cpp \
             src/modules/administration/service/administration_service.cpp

HEADERS := $(shell find external/protocol/include src -name '*.hpp' 2>/dev/null | sort)

$(BUILD)/platform_paths_test: $(PLATFORM_SRC) tests/platform/paths_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/platform_logging_test: $(LOGGING_SRC) tests/platform/logging_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/platform_crash_test: $(LOGGING_SRC) $(CRASH_SRC) tests/platform/crash_handler_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/platform_single_instance_test: $(SINGLE_INSTANCE_SRC) tests/platform/single_instance_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(SODIUM_LIB): external/dist/libsodium-1.0.22.tar.gz
>@rm -rf build/vendor/libsodium-src $(SODIUM_PREFIX)
>@mkdir -p build/vendor/libsodium-src $(SODIUM_PREFIX)
>@tar -xzf $< -C build/vendor/libsodium-src --strip-components=1
>@cd build/vendor/libsodium-src && ./configure --disable-shared --enable-static --prefix=$(abspath $(SODIUM_PREFIX)) >/dev/null
>@$(MAKE) -C build/vendor/libsodium-src -j2 >/dev/null
>@$(MAKE) -C build/vendor/libsodium-src install >/dev/null

$(BUILD)/platform_secrets_test: $(SECRETS_SRC) tests/platform/secrets_test.cpp $(SODIUM_LIB)
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $(SECRETS_SRC) tests/platform/secrets_test.cpp $(SODIUM_LIB) -o $@

$(BUILD)/platform_network_state_test: $(NETWORK_STATE_SRC) tests/platform/network_state_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/platform_background_services_test: $(BACKGROUND_SERVICES_SRC) tests/platform/background_services_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_startup_test: $(APP_STARTUP_SRC) tests/app/startup_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) -Iexternal/protocol/include $^ -o $@

$(BUILD)/app_composition_root_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                                    $(PAYLOAD_SRC) $(MODULES_SRC) $(ADMIN_SRC) $(PARTIES_SRC) \
                                    $(CATALOG_SRC) $(PRICING_SRC) $(ORDERS_SRC) $(RECEIVABLES_SRC) \
                                    $(JOBS_SRC) $(QUOTATIONS_SRC) $(AGREEMENTS_SRC) $(SOURCING_SRC) \
                                    $(COMPANION_SRC) $(FILES_SRC) $(ALL_WORKFLOWS_SRC) \
                                    src/app/composition_root.cpp tests/app/composition_root_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) -Iexternal/protocol/include $^ -o $@

$(BUILD)/app_real_startup_runtime_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                                       $(PAYLOAD_SRC) $(MODULES_SRC) $(ADMIN_SRC) $(PARTIES_SRC) \
                                       $(CATALOG_SRC) $(PRICING_SRC) $(ORDERS_SRC) $(RECEIVABLES_SRC) \
                                       $(JOBS_SRC) $(QUOTATIONS_SRC) $(AGREEMENTS_SRC) $(SOURCING_SRC) \
                                       $(COMPANION_SRC) $(FILES_SRC) $(ALL_WORKFLOWS_SRC) \
                                       src/app/startup.cpp src/app/startup_sequence.cpp \
                                       src/app/composition_root.cpp src/app/real_startup_runtime.cpp \
                                       tests/app/real_startup_runtime_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) -Iexternal/protocol/include $^ -o $@

$(BUILD)/app_contracts_test: tests/app/contracts_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_dashboard_service_test: $(PROTOCOL_SRC) src/engine/identity/rights_set.cpp src/app/dashboard/dashboard_service.cpp tests/app/dashboard_service_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_primary_page_service_test: $(PROTOCOL_SRC) src/engine/identity/rights_set.cpp \
                                        src/app/primary/primary_page_service.cpp \
                                        tests/app/primary_page_service_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_local_primary_query_test: $(STORAGE_SRC) src/engine/records/lifecycle.cpp \
                                       src/app/primary/local_primary_query.cpp \
                                       tests/app/local_primary_query_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_record_page_service_test: $(PROTOCOL_SRC) src/engine/identity/rights_set.cpp \
                                       src/engine/records/identity.cpp \
                                       src/app/primary/primary_page_service.cpp \
                                       src/app/primary/record_page_service.cpp \
                                       tests/app/record_page_service_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_local_record_query_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) \
                                      src/modules/administration/domain/person.cpp \
                                      src/modules/administration/domain/device.cpp \
                                      src/modules/administration/data/repository.cpp \
                                      src/modules/parties/domain/party.cpp \
                                      src/modules/parties/data/repository.cpp \
                                      src/modules/catalog/domain/product.cpp \
                                      src/modules/catalog/data/repository.cpp \
                                      src/modules/pricing/domain/rate.cpp \
                                      src/modules/pricing/data/repository.cpp \
                                      src/modules/orders/domain/order.cpp \
                                      src/modules/orders/data/repository.cpp \
                                      src/modules/receivables/domain/invoice.cpp \
                                      src/modules/receivables/domain/payment.cpp \
                                      src/modules/receivables/domain/credit_account.cpp \
                                      src/modules/receivables/domain/document_number_block.cpp \
                                      src/modules/receivables/domain/document_delivery.cpp \
                                      src/modules/receivables/domain/statement.cpp \
                                      src/modules/receivables/data/repository.cpp \
                                      src/modules/jobs/domain/job.cpp \
                                      src/modules/jobs/data/repository.cpp \
                                      src/modules/quotations/domain/quotation.cpp \
                                      src/modules/quotations/data/repository.cpp \
                                      src/modules/agreements/domain/agreement.cpp \
                                      src/modules/agreements/domain/consumption.cpp \
                                      src/modules/agreements/data/repository.cpp \
                                      src/modules/sourcing/domain/sourcing.cpp \
                                      src/modules/sourcing/data/repository.cpp \
                                      src/modules/companion/domain/task.cpp \
                                      src/modules/companion/data/repository.cpp \
                                      src/modules/files/domain/file_asset.cpp \
                                      src/modules/files/data/repository.cpp \
                                      src/app/primary/local_record_query.cpp \
                                      tests/app/local_record_query_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_command_gateway_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) $(MODULES_SRC) \
                                   src/app/session_authorization.cpp \
                                   src/app/primary/command_gateway.cpp \
                                   tests/app/command_gateway_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_session_authorization_test: $(PROTOCOL_SRC) src/engine/identity/rights_set.cpp \
                                         src/app/session_authorization.cpp \
                                         tests/app/session_authorization_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/app_workspace_runtime_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                                     $(PAYLOAD_SRC) $(MODULES_SRC) $(ADMIN_SRC) \
                                     src/modules/parties/domain/party.cpp \
                                     src/modules/parties/data/repository.cpp \
                                     src/modules/catalog/domain/product.cpp \
                                     src/modules/catalog/data/repository.cpp \
                                     src/modules/pricing/domain/rate.cpp \
                                     src/modules/pricing/data/repository.cpp \
                                     src/modules/orders/domain/order.cpp \
                                     src/modules/orders/data/repository.cpp \
                                     src/modules/receivables/domain/invoice.cpp \
                                     src/modules/receivables/domain/payment.cpp \
                                     src/modules/receivables/domain/credit_account.cpp \
                                     src/modules/receivables/domain/document_number_block.cpp \
                                     src/modules/receivables/domain/document_delivery.cpp \
                                     src/modules/receivables/domain/statement.cpp \
                                     src/modules/receivables/data/repository.cpp \
                                     src/modules/jobs/domain/job.cpp \
                                     src/modules/jobs/data/repository.cpp \
                                     src/modules/quotations/domain/quotation.cpp \
                                     src/modules/quotations/data/repository.cpp \
                                     src/modules/agreements/domain/agreement.cpp \
                                     src/modules/agreements/domain/consumption.cpp \
                                     src/modules/agreements/data/repository.cpp \
                                     src/modules/sourcing/domain/sourcing.cpp \
                                     src/modules/sourcing/data/repository.cpp \
                                     src/modules/companion/domain/task.cpp \
                                     src/modules/companion/data/repository.cpp \
                                     src/modules/files/domain/file_asset.cpp \
                                     src/modules/files/data/repository.cpp \
                                     src/app/primary/local_primary_query.cpp \
                                     src/app/primary/local_record_query.cpp \
                                     src/app/primary/primary_page_service.cpp \
                                     src/app/primary/record_page_service.cpp \
                                     src/app/primary/command_gateway.cpp \
                                     src/app/session_authorization.cpp \
                                     src/app/workspace_runtime.cpp \
                                     tests/app/workspace_runtime_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_presentation_seam_test: src/shell/completion_gate.cpp tests/shell/presentation_seam_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_primitive_mapping_test: tests/shell/primitive_mapping_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_surface_lifecycle_test: src/shell/surface_lifecycle.cpp src/app/startup.cpp tests/shell/surface_lifecycle_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_screen_registry_test: $(PROTOCOL_SRC) src/shell/screen_registry.cpp tests/shell/screen_registry_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) -Iexternal/protocol/include $^ -o $@

$(BUILD)/shell_navigation_manifest_test: $(PROTOCOL_SRC) src/shell/screen_registry.cpp src/shell/paged_list_cache.cpp src/shell/list_bridge.cpp src/shell/navigation_manifest.cpp tests/shell/navigation_manifest_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) -Iexternal/protocol/include $^ -o $@

$(BUILD)/shell_navigation_controller_test: $(PROTOCOL_SRC) src/shell/screen_registry.cpp src/shell/navigation_controller.cpp tests/shell/navigation_controller_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) -Iexternal/protocol/include $^ -o $@

$(BUILD)/shell_paged_list_cache_test: src/shell/paged_list_cache.cpp tests/shell/paged_list_cache_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_list_bridge_test: src/shell/paged_list_cache.cpp src/shell/list_bridge.cpp tests/shell/list_bridge_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_form_bridge_test: src/shell/form_bridge.cpp tests/shell/form_bridge_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_document_snapshot_test: src/shell/document_snapshot.cpp tests/shell/document_snapshot_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_thumbnail_cache_test: src/shell/thumbnail_cache.cpp tests/shell/thumbnail_cache_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_window_state_test: src/shell/window_state.cpp tests/shell/window_state_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_dashboard_bridge_test: src/shell/dashboard_bridge.cpp tests/shell/dashboard_bridge_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/shell_state_test: src/shell/notification_queue.cpp src/shell/shell_state.cpp tests/shell/shell_state_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

SERVER_IDENTITY_SRC := server/src/identity/token_store.cpp \
                       server/src/identity/token_issuer.cpp \
                       src/engine/records/identity.cpp

$(BUILD)/server_identity_token_issuer_test: $(SERVER_IDENTITY_SRC) server/tests/identity/token_issuer_test.cpp $(SODIUM_LIB)
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) -Iserver/src $(SERVER_IDENTITY_SRC) server/tests/identity/token_issuer_test.cpp $(SODIUM_LIB) -o $@

$(BUILD)/app_domain_event_bus_test: tests/app/domain_event_bus_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/protocol_test: $(PROTOCOL_SRC) external/protocol/tests/protocol_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/engine_test: $(PROTOCOL_SRC) $(ENGINE_SRC) tests/engine/engine_domain_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/storage_test: $(STORAGE_SRC) tests/engine/engine_storage_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/migration_test: $(STORAGE_SRC) tests/engine/engine_migration_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/writer_test: $(STORAGE_SRC) tests/engine/engine_writer_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/outbox_test: $(PROTOCOL_SRC) $(STORAGE_SRC) $(SYNC_SRC) tests/engine/engine_outbox_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/sync_test: $(PROTOCOL_SRC) $(STORAGE_SRC) $(SYNC_SRC) tests/engine/engine_sync_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/payload_test: $(STORAGE_SRC) src/engine/records/payload.cpp \
                       tests/engine/engine_payload_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/framework_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                         $(MODULES_SRC) tests/modules/module_framework_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/workflow_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                        $(MODULES_SRC) tests/workflows/workflow_framework_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/quote_to_order_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                              $(PAYLOAD_SRC) $(MODULES_SRC) $(PRICING_SRC) $(ORDERS_SRC) \
                              $(QUOTATIONS_SRC) $(QUOTE_TO_ORDER_SRC) \
                              tests/workflows/quote_to_order_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/order_to_jobs_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                             $(PAYLOAD_SRC) $(MODULES_SRC) $(PRICING_SRC) $(ORDERS_SRC) \
                             $(JOBS_SRC) $(ORDER_TO_JOBS_SRC) \
                             tests/workflows/order_to_jobs_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/issue_invoice_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                             $(PAYLOAD_SRC) $(MODULES_SRC) $(PARTIES_SRC) $(PRICING_SRC) $(ORDERS_SRC) \
                             $(RECEIVABLES_SRC) $(AGREEMENTS_SRC) $(ISSUE_INVOICE_SRC) \
                             tests/workflows/issue_invoice_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/cancel_and_reissue_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                                  $(PAYLOAD_SRC) $(MODULES_SRC) $(PARTIES_SRC) $(PRICING_SRC) $(ORDERS_SRC) \
                                  $(RECEIVABLES_SRC) $(AGREEMENTS_SRC) $(ISSUE_INVOICE_SRC) $(CANCEL_REISSUE_SRC) \
                                  tests/workflows/cancel_and_reissue_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/take_payment_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                            $(PAYLOAD_SRC) $(MODULES_SRC) $(PARTIES_SRC) $(PRICING_SRC) \
                            $(RECEIVABLES_SRC) $(TAKE_PAYMENT_SRC) \
                            tests/workflows/take_payment_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/counter_sale_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                            $(PAYLOAD_SRC) $(MODULES_SRC) $(PARTIES_SRC) $(PRICING_SRC) \
                            $(ORDERS_SRC) $(RECEIVABLES_SRC) $(COUNTER_SALE_SRC) \
                            tests/workflows/counter_sale_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/apply_agreement_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                              $(PAYLOAD_SRC) $(MODULES_SRC) $(PARTIES_SRC) $(PRICING_SRC) $(ORDERS_SRC) \
                              $(RECEIVABLES_SRC) $(AGREEMENTS_SRC) $(ISSUE_INVOICE_SRC) \
                              $(CANCEL_REISSUE_SRC) $(APPLY_AGREEMENT_SRC) \
                              tests/workflows/apply_agreement_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/document_delivery_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                                 $(PAYLOAD_SRC) $(MODULES_SRC) $(PARTIES_SRC) $(PRICING_SRC) \
                                 $(RECEIVABLES_SRC) $(QUOTATIONS_SRC) $(AGREEMENTS_SRC) \
                                 $(DOCUMENT_DELIVERY_SRC) tests/workflows/document_delivery_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/record_purchase_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                                $(PAYLOAD_SRC) $(MODULES_SRC) $(SOURCING_SRC) \
                                $(RECORD_PURCHASE_SRC) tests/workflows/record_purchase_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/administration_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                              $(PAYLOAD_SRC) $(MODULES_SRC) $(ADMIN_SRC) \
                              src/modules/administration/tests/administration_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/parties_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                       $(PAYLOAD_SRC) $(MODULES_SRC) $(PARTIES_SRC) \
                       src/modules/parties/tests/parties_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/catalog_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                       $(PAYLOAD_SRC) $(MODULES_SRC) $(CATALOG_SRC) \
                       src/modules/catalog/tests/catalog_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/pricing_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                       $(PAYLOAD_SRC) $(MODULES_SRC) $(PRICING_SRC) \
                       src/modules/pricing/tests/pricing_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/orders_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                      $(PAYLOAD_SRC) $(MODULES_SRC) $(PRICING_SRC) $(ORDERS_SRC) \
                      src/modules/orders/tests/orders_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/receivables_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                          $(PAYLOAD_SRC) $(MODULES_SRC) $(PARTIES_SRC) \
                          $(PRICING_SRC) $(RECEIVABLES_SRC) \
                          src/modules/receivables/tests/receivables_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/jobs_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                   $(PAYLOAD_SRC) $(MODULES_SRC) $(JOBS_SRC) \
                   src/modules/jobs/tests/jobs_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/quotations_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                         $(PAYLOAD_SRC) $(MODULES_SRC) $(QUOTATIONS_SRC) \
                         src/modules/quotations/tests/quotations_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/agreements_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                         $(PAYLOAD_SRC) $(MODULES_SRC) $(AGREEMENTS_SRC) \
                         src/modules/agreements/tests/agreements_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/sourcing_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                       $(PAYLOAD_SRC) $(MODULES_SRC) $(SOURCING_SRC) \
                       src/modules/sourcing/tests/sourcing_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/companion_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                        $(PAYLOAD_SRC) $(MODULES_SRC) $(COMPANION_SRC) \
                        src/modules/companion/tests/companion_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/files_test: $(PROTOCOL_SRC) $(ENGINE_SRC) $(STORAGE_SRC) $(SYNC_SRC) \
                    $(PAYLOAD_SRC) $(MODULES_SRC) $(FILES_SRC) \
                    src/modules/files/tests/files_test.cpp
>@mkdir -p $(BUILD)
>$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: check tests headers integrity architecture standard clean

check: standard integrity architecture headers tests

standard:
>@echo '== language =='
>@mkdir -p $(BUILD)
>@printf '#include <version>\n#include <cstdio>\nint main(){std::printf("__cplusplus %%ld\\n",(long)__cplusplus);}\n' > $(BUILD)/.std.cpp
>@$(CXX) -std=$(STD) $(BUILD)/.std.cpp -o $(BUILD)/.std && ./$(BUILD)/.std
>@echo "compiler   $$($(CXX) --version | head -1)"
>@echo "standard   $(STD)"
>@echo

# Every header must compile on its own, with nothing included before it, and
# must survive being included twice. A header that only works because some
# other header came first is a trap for whoever includes it next.
headers:
>@echo '== header self-containment =='
>@mkdir -p $(BUILD)
>@count=0; bad=0; \
> for header in $(HEADERS); do \
>   count=$$((count+1)); \
>   printf '#include "%s"\n#include "%s"\n' "$$header" "$$header" > $(BUILD)/.selftest.cpp; \
>   if ! $(CXX) $(CXXFLAGS) -I. -fsyntax-only $(BUILD)/.selftest.cpp 2> $(BUILD)/.selftest.log; then \
>     echo "  FAIL $$header"; sed 's/^/        /' $(BUILD)/.selftest.log | head -6; \
>     bad=$$((bad+1)); \
>   fi; \
> done; \
> echo "  $$count headers, $$bad not self-contained"; \
> echo; \
> test $$bad -eq 0

integrity:
>@echo '== integrity =='
>@$(PYTHON) tools/sandbox/integrity.py
>@echo

architecture:
>@echo '== QML architecture boundary =='
>@$(PYTHON) tools/sandbox/check_application_architecture.py
>@$(PYTHON) tools/sandbox/check_composition_root.py
>@$(PYTHON) tools/sandbox/check_module_boundaries.py
>@$(PYTHON) tools/sandbox/check_server_provider_boundaries.py
>@$(PYTHON) tools/sandbox/check_extension_boundaries.py
>@$(PYTHON) tools/sandbox/check_qml_boundary.py
>@$(PYTHON) tools/sandbox/test_qml_boundary.py
>@$(PYTHON) tools/sandbox/check_ui_resources.py
>@$(PYTHON) tools/sandbox/check_qt_bridge.py
>@$(PYTHON) tools/sandbox/check_navigation_and_lists.py
>@$(PYTHON) tools/sandbox/check_dashboard_and_shell.py
>@$(PYTHON) tools/sandbox/check_phase78_design_policy.py
>@echo

tests: $(BUILD)/platform_paths_test $(BUILD)/platform_logging_test $(BUILD)/platform_crash_test $(BUILD)/platform_single_instance_test $(BUILD)/platform_secrets_test $(BUILD)/platform_network_state_test $(BUILD)/platform_background_services_test $(BUILD)/app_startup_test $(BUILD)/app_composition_root_test $(BUILD)/app_real_startup_runtime_test $(BUILD)/app_contracts_test $(BUILD)/app_dashboard_service_test $(BUILD)/app_primary_page_service_test $(BUILD)/app_local_primary_query_test $(BUILD)/app_record_page_service_test $(BUILD)/app_local_record_query_test $(BUILD)/app_command_gateway_test $(BUILD)/app_session_authorization_test $(BUILD)/app_workspace_runtime_test $(BUILD)/protocol_test $(BUILD)/engine_test $(BUILD)/storage_test $(BUILD)/shell_presentation_seam_test $(BUILD)/shell_primitive_mapping_test $(BUILD)/shell_surface_lifecycle_test $(BUILD)/shell_screen_registry_test $(BUILD)/shell_navigation_manifest_test $(BUILD)/shell_navigation_controller_test $(BUILD)/shell_paged_list_cache_test $(BUILD)/shell_list_bridge_test $(BUILD)/shell_form_bridge_test $(BUILD)/shell_document_snapshot_test $(BUILD)/shell_thumbnail_cache_test $(BUILD)/shell_window_state_test $(BUILD)/shell_dashboard_bridge_test $(BUILD)/shell_state_test $(BUILD)/server_identity_token_issuer_test $(BUILD)/app_domain_event_bus_test \
       $(BUILD)/migration_test $(BUILD)/writer_test $(BUILD)/outbox_test \
       $(BUILD)/sync_test $(BUILD)/payload_test $(BUILD)/framework_test \
       $(BUILD)/workflow_test $(BUILD)/quote_to_order_test \
       $(BUILD)/order_to_jobs_test $(BUILD)/administration_test $(BUILD)/parties_test \
       $(BUILD)/issue_invoice_test \
       $(BUILD)/cancel_and_reissue_test \
       $(BUILD)/take_payment_test \
       $(BUILD)/counter_sale_test \
       $(BUILD)/apply_agreement_test \
       $(BUILD)/record_purchase_test \
       $(BUILD)/document_delivery_test \
       $(BUILD)/catalog_test $(BUILD)/pricing_test $(BUILD)/orders_test \
       $(BUILD)/receivables_test $(BUILD)/jobs_test \
       $(BUILD)/quotations_test \
       $(BUILD)/agreements_test \
       $(BUILD)/sourcing_test \
       $(BUILD)/companion_test \
       $(BUILD)/files_test
>@echo '== platform paths =='
>./$(BUILD)/platform_paths_test
>@echo
>@echo '== platform logging =='
>./$(BUILD)/platform_logging_test
>@echo
>@echo '== platform crash handling =='
>./$(BUILD)/platform_crash_test
>@echo
>@echo '== platform single instance =='
>./$(BUILD)/platform_single_instance_test
>@echo
>@echo '== platform secrets =='
>./$(BUILD)/platform_secrets_test
>@echo
>@echo '== platform network state =='
>./$(BUILD)/platform_network_state_test
>@echo
>@echo '== platform background services =='
>./$(BUILD)/platform_background_services_test
>@echo
>@echo '== application startup =='
>./$(BUILD)/app_startup_test
>@echo
>@echo '== application composition root =='
>./$(BUILD)/app_composition_root_test
>@echo
>@echo '== real startup runtime =='
>./$(BUILD)/app_real_startup_runtime_test
>@echo
>@echo '== application contracts =='
>./$(BUILD)/app_contracts_test
>@echo
>@echo '== application dashboard service =='
>./$(BUILD)/app_dashboard_service_test
>@echo
>@echo '== application primary page service =='
>./$(BUILD)/app_primary_page_service_test
>@echo
>@echo '== application local primary query =='
>./$(BUILD)/app_local_primary_query_test
>@echo
>@echo '== application record page service =='
>./$(BUILD)/app_record_page_service_test
>@echo
>@echo '== application local record query =='
>./$(BUILD)/app_local_record_query_test
>@echo
>@echo '== application command gateway =='
>./$(BUILD)/app_command_gateway_test
>@echo
>@echo '== session authorization =='
>./$(BUILD)/app_session_authorization_test
>@echo
>@echo '== authenticated workspace runtime =='
>./$(BUILD)/app_workspace_runtime_test
>@echo
>@echo '== primitive mapping =='
>./$(BUILD)/shell_primitive_mapping_test
>@echo
>@echo '== presentation seam =='
>./$(BUILD)/shell_presentation_seam_test
>@echo
>@echo '== surface lifecycle =='
>./$(BUILD)/shell_surface_lifecycle_test
>@echo
>@echo '== screen registry =='
>./$(BUILD)/shell_screen_registry_test
>@echo
>@echo '== navigation manifest =='
>./$(BUILD)/shell_navigation_manifest_test
>@echo
>@echo '== navigation controller =='
>./$(BUILD)/shell_navigation_controller_test
>@echo
>@echo '== paged list cache =='
>./$(BUILD)/shell_paged_list_cache_test
>@echo
>@echo '== list bridge =='
>./$(BUILD)/shell_list_bridge_test
>@echo
>@echo '== forms =='
>./$(BUILD)/shell_form_bridge_test
>@echo
>@echo '== document snapshots =='
>./$(BUILD)/shell_document_snapshot_test
>@echo
>@echo '== thumbnails =='
>./$(BUILD)/shell_thumbnail_cache_test
>@echo
>@echo '== window state =='
>./$(BUILD)/shell_window_state_test
>@echo
>@echo '== dashboard bridge =='
>./$(BUILD)/shell_dashboard_bridge_test
>@echo
>@echo '== shell state and notifications =='
>./$(BUILD)/shell_state_test
>@echo
>@echo '== server identity: token issuer =='
>./$(BUILD)/server_identity_token_issuer_test
>@echo
>@echo '== domain event bus =='
>./$(BUILD)/app_domain_event_bus_test
>@echo
>@echo '== protocol =='
>./$(BUILD)/protocol_test
>@echo
>@echo '== engine =='
>./$(BUILD)/engine_test
>@echo
>@echo '== storage =='
>./$(BUILD)/storage_test
>@echo
>@echo '== migrations =='
>./$(BUILD)/migration_test
>@echo
>@echo '== writer gate =='
>./$(BUILD)/writer_test
>@echo
>@echo '== outbox =='
>./$(BUILD)/outbox_test
>@echo
>@echo '== cursor and conflict =='
>./$(BUILD)/sync_test
>@echo
>@echo '== call payloads =='
>./$(BUILD)/payload_test
>@echo
>@echo '== module framework =='
>./$(BUILD)/framework_test
>@echo
>@echo '== workflow framework =='
>./$(BUILD)/workflow_test
>@echo
>@echo '== quote to order workflow =='
>./$(BUILD)/quote_to_order_test
>@echo
>@echo '== order to jobs workflow =='
>./$(BUILD)/order_to_jobs_test
>@echo
>@echo '== issue invoice workflow =='
>./$(BUILD)/issue_invoice_test
>@echo
>@echo '== cancel and reissue workflow =='
>./$(BUILD)/cancel_and_reissue_test
>@echo
>@echo '== take payment workflow =='
>./$(BUILD)/take_payment_test
>@echo
>@echo '== counter sale workflow =='
>./$(BUILD)/counter_sale_test
>@echo
>@echo '== apply agreement workflow =='
>./$(BUILD)/apply_agreement_test
>@echo
>@echo '== document delivery workflow =='
>./$(BUILD)/document_delivery_test
>@echo
>@echo '== record purchase workflow =='
>./$(BUILD)/record_purchase_test
>@echo
>@echo '== administration =='
>./$(BUILD)/administration_test
>@echo
>@echo '== parties =='
>./$(BUILD)/parties_test
>@echo
>@echo '== catalog =='
>./$(BUILD)/catalog_test
>@echo
>@echo '== pricing =='
>./$(BUILD)/pricing_test
>@echo
>@echo '== orders =='
>./$(BUILD)/orders_test
>@echo
>@echo '== receivables =='
>./$(BUILD)/receivables_test
>@echo
>@echo '== jobs =='
>./$(BUILD)/jobs_test
>@echo
>@echo '== quotations =='
>./$(BUILD)/quotations_test
>@echo
>@echo '== agreements =='
>./$(BUILD)/agreements_test
>@echo
>@echo '== sourcing =='
>./$(BUILD)/sourcing_test
>@echo
>@echo '== companion =='
>./$(BUILD)/companion_test
>@echo
>@echo '== files =='
>./$(BUILD)/files_test

clean:
>rm -rf $(BUILD)

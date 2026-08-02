#include <squiflow/protocol/operation_table.hpp>

#include <squiflow/protocol/module_graph.hpp>

#include <cstdio>
#include <cstdlib>

namespace squiflow::protocol {
namespace {

constexpr OperationInfo kOperations[] = {
#define SQF_OPERATION(name, module, right, cls, offline)     \
    OperationInfo{OperationId::name,                         \
                  #name,                                     \
                  ModuleId::module,                          \
                  RightId::right,                            \
                  OperationClass::cls,                       \
                  OfflineRule::offline},
#include <squiflow/protocol/operations.def>
#undef SQF_OPERATION
};

static_assert(std::size(kOperations) == kOperationCount,
              "the operation table and the operation enum disagree");

constexpr std::string_view kRightNames[] = {
#define SQF_RIGHT(name, module) #name,
#include <squiflow/protocol/rights.def>
#undef SQF_RIGHT
};

constexpr ModuleId kRightModules[] = {
#define SQF_RIGHT(name, module) ModuleId::module,
#include <squiflow/protocol/rights.def>
#undef SQF_RIGHT
};

static_assert(std::size(kRightNames) == kRightCount,
              "the right name table and the right enum disagree");
static_assert(std::size(kRightModules) == kRightCount,
              "the right module table and the right enum disagree");

// Reading past the end of the operation or right table would answer a
// question about permission or connectivity with whatever bytes follow it.
// For a program that moves money, stopping is the correct answer.
[[noreturn]] void fail(const char* what, std::size_t value, std::size_t limit) noexcept {
    std::fprintf(stderr, "squiflow protocol: %s (value %zu, this build has %zu)\n",
                 what, value, limit);
    std::abort();
}

}  // namespace

std::string_view to_string(OperationClass value) noexcept {
    switch (value) {
        case OperationClass::LocalOnly:
            return "LocalOnly";
        case OperationClass::Synchronizable:
            return "Synchronizable";
        case OperationClass::OnlineRequired:
            return "OnlineRequired";
    }
    return "?";
}

std::string_view to_string(OfflineRule value) noexcept {
    switch (value) {
        case OfflineRule::OfflineAllowed:
            return "OfflineAllowed";
        case OfflineRule::OnlineOnly:
            return "OnlineOnly";
    }
    return "?";
}

std::string_view right_name(RightId right) noexcept {
    if (!is_valid(right)) {
        fail("right_name on a right this build does not have",
             static_cast<std::size_t>(right), kRightCount);
    }
    return kRightNames[static_cast<std::size_t>(right)];
}

ModuleId right_module(RightId right) noexcept {
    if (!is_valid(right)) {
        fail("right_module on a right this build does not have",
             static_cast<std::size_t>(right), kRightCount);
    }
    return kRightModules[static_cast<std::size_t>(right)];
}

std::span<const OperationInfo> all_operations() noexcept {
    return std::span<const OperationInfo>(kOperations, std::size(kOperations));
}

const OperationInfo& operation(OperationId id) noexcept {
    if (!is_valid(id)) {
        fail("operation() on an operation this build does not have",
             static_cast<std::size_t>(id), kOperationCount);
    }
    return kOperations[static_cast<std::size_t>(id)];
}

const OperationInfo* try_operation(OperationId id) noexcept {
    if (!is_valid(id)) {
        return nullptr;
    }
    return &kOperations[static_cast<std::size_t>(id)];
}

const OperationInfo* find_operation(std::string_view name) noexcept {
    for (const OperationInfo& info : kOperations) {
        if (info.name == name) {
            return &info;
        }
    }
    return nullptr;
}

const OperationInfo* find_operation(std::uint32_t number) noexcept {
    if (number >= kOperationCount) {
        return nullptr;
    }
    return &kOperations[number];
}

bool allowed_offline(OperationId id) noexcept {
    return operation(id).offline == OfflineRule::OfflineAllowed;
}

bool staff_offline_exception(OperationId id) noexcept {
    switch (id) {
#define SQF_STAFF_OFFLINE(name) case OperationId::name:
#include <squiflow/protocol/staff_offline.def>
#undef SQF_STAFF_OFFLINE
        return true;
    default:
        return false;
    }
}

}  // namespace squiflow::protocol

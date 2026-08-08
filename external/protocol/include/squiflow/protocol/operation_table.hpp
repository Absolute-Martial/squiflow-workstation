#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <squiflow/protocol/module_id.hpp>
#include <squiflow/protocol/operation_class.hpp>
#include <squiflow/protocol/right_id.hpp>

namespace squiflow::protocol {

enum class OperationId : std::uint16_t {
#if !defined(Q_MOC_RUN)
#define SQF_OPERATION(name, module, right, cls, offline) name,
#include <squiflow/protocol/operations.def>
#undef SQF_OPERATION
#endif
    Count
};

inline constexpr std::size_t kOperationCount =
    static_cast<std::size_t>(OperationId::Count);

// True only for an operation this build actually has. Two devices are not
// always on the same build, so a number that means something on one of them
// may mean nothing here.
constexpr bool is_valid(OperationId id) noexcept {
    return static_cast<std::size_t>(id) < kOperationCount;
}

struct OperationInfo {
    OperationId id;
    std::string_view name;
    ModuleId module;
    RightId right;
    OperationClass sync_class;
    OfflineRule offline;
};

std::span<const OperationInfo> all_operations() noexcept;

// Aborts rather than read out of bounds when handed an invalid identifier.
// Anything that came from outside this program must be resolved with
// find_operation first; by the time a bad identifier reaches this, the
// program state is already wrong and continuing would corrupt records.
const OperationInfo& operation(OperationId id) noexcept;

// Null when the identifier names nothing in this build. Use this instead of
// operation() wherever the value did not come from a literal enumerator.
const OperationInfo* try_operation(OperationId id) noexcept;

// Null when no operation carries that name. Used when a sync payload arrives
// naming something this build does not know about.
const OperationInfo* find_operation(std::string_view name) noexcept;

// Null when the number names no operation in this build. This is the other
// half of the wire boundary: payloads may identify an operation by number as
// well as by name, and a number is trivially castable to OperationId, so
// nothing else in the program should perform that cast.
const OperationInfo* find_operation(std::uint32_t number) noexcept;

// Whether an operation may be started with no connection. The answer is data,
// not a condition scattered across screens. Aborts on an invalid identifier,
// for the same reason operation() does.
bool allowed_offline(OperationId id) noexcept;

// Staff are read-only when this device has no connection. These few operations
// are the exceptions, because refusing them would mean turning away a customer
// standing at the counter. The owner is not subject to the read-only rule at
// all, so this answer is only consulted for staff.
//
// Safe for any value: an identifier this build does not know about is not an
// exception, which is the refusing answer.
bool staff_offline_exception(OperationId id) noexcept;

}  // namespace squiflow::protocol

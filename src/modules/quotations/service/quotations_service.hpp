#pragma once

// The write side of quotations: five handlers, one per operation on the wire.
//
// The protocol declares no line-level operation for this module, unlike orders
// with its order_line_add. That is not an omission to work around: a quotation
// is offered as a whole document, so its lines arrive with the document. They
// are carried in the payload as indexed fields - line_count, then line.0.id,
// line.0.description and so on - and the whole set is written in one
// transaction or none of it is.

#include <cstdint>
#include <functional>

#include "engine/storage/store.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::quotations {

// A damaged or hostile payload must not be able to make this process allocate
// an arbitrary number of lines before any rule is checked. No real quotation
// approaches this; a corrupted length field does.
inline constexpr std::int64_t kMaxLines = 500;

class QuotationsService {
public:
    using Clock = std::function<std::int64_t()>;

    explicit QuotationsService(Clock clock) : clock_{std::move(clock)} {}

    void create(engine::Transaction& transaction, const Call& call) const;
    void revise(engine::Transaction& transaction, const Call& call) const;
    void issue(engine::Transaction& transaction, const Call& call) const;
    void accept(engine::Transaction& transaction, const Call& call) const;
    void expire(engine::Transaction& transaction, const Call& call) const;

private:
    Clock clock_;
};

}  // namespace squiflow::modules::quotations

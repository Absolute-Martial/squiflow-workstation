#pragma once

#include <cstdint>
#include <string>

#include "engine/storage/store.hpp"

namespace squiflow::modules::agreements {

enum class ConsumptionState : std::uint8_t { Active, Released };

struct AgreementConsumption {
    std::string id{};
    std::string agreement_id{};
    std::string agreement_line_id{};
    std::string invoice_id{};
    std::string invoice_line_id{};
    std::int64_t quantity_scaled{0};
    ConsumptionState state{ConsumptionState::Active};
    std::int64_t consumed_at{0};
    std::string consumed_by{};
    std::int64_t released_at{0};
    std::string released_by{};
    std::string release_reason{};
};

std::string consumption_id(const std::string& invoice_line_id,
                           const std::string& agreement_line_id);
void validate(const AgreementConsumption& consumption);
engine::Row to_row(const AgreementConsumption& consumption);
AgreementConsumption consumption_from_row(const engine::Row& row);

}  // namespace squiflow::modules::agreements

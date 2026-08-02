#pragma once

#include <cstdint>
#include <string>

#include "engine/storage/store.hpp"

namespace squiflow::modules::catalog {

// A product is identified by the name the shop uses for it. Descriptions and
// notes are optional. Nothing in this module stores a price; prices belong in
// the pricing module, because a rate is a relation between product, party,
// agreement and time, not an attribute of a product.
struct Product {
    std::string id{};
    std::string name{};
    std::string description{};
    bool archived{false};
    std::int64_t created_at{0};
    std::int64_t updated_at{0};
    std::string created_by{};
    std::string updated_by{};
};

void validate(const Product& product);
engine::Row to_row(const Product& product);
Product product_from_row(const engine::Row& row);

}  // namespace squiflow::modules::catalog

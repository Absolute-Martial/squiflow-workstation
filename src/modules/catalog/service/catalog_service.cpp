#include "modules/catalog/service/catalog_service.hpp"

#include <stdexcept>

#include "engine/records/payload.hpp"
#include "modules/catalog/data/repository.hpp"
#include "modules/catalog/domain/product.hpp"

namespace squiflow::modules::catalog {
namespace {

std::string required_text(const engine::Row& fields, const std::string& name,
                          const std::string& complaint) {
    const std::string value = fields.get(name).text_or({});
    bool blank = true;
    for (const char c : value)
        if (static_cast<unsigned char>(c) > ' ') { blank = false; break; }
    if (blank) throw RuleViolation(complaint);
    return value;
}

std::string subject(const Call& call) {
    if (call.record_id.empty())
        throw RuleViolation("This request does not say which record it is about.");
    return call.record_id;
}

}  // namespace

engine::Row read_fields(const Call& call) {
    if (call.payload.empty()) return {};
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError& broken) {
        throw RuleViolation(std::string("This request could not be read: ") + broken.what());
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr)
        throw std::logic_error("a handler was run without a session");
    return *call.actor;
}

CatalogService::CatalogService(Clock clock) : clock_(std::move(clock)) {
    if (!clock_) throw std::logic_error("catalog needs a clock");
}

void CatalogService::create_product(engine::Transaction& tx, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string id     = subject(call);

    if (data::find_product(tx, id)) {
        throw RuleViolation("That product already exists.");
    }

    Product product;
    product.id          = id;
    product.name        = required_text(fields, "name", "A product needs a name.");
    product.description = fields.get("description").text_or({});
    product.created_at  = clock_();
    product.updated_at  = product.created_at;
    product.created_by  = engine::to_string(actor(call).person);
    product.updated_by  = product.created_by;
    validate(product);

    data::save_product(tx, product);
}

void CatalogService::update_product(engine::Transaction& tx, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string id     = subject(call);

    std::optional<Product> existing = data::find_product(tx, id);
    if (!existing)         throw RuleViolation("That product is not in the catalog.");
    if (existing->archived) throw RuleViolation("Unarchive this product before changing it.");

    Product updated = *existing;
    if (fields.has("name"))
        updated.name = required_text(fields, "name", "A product needs a name.");
    if (fields.has("description"))
        updated.description = fields.get("description").text_or({});
    updated.updated_at = clock_();
    updated.updated_by = engine::to_string(actor(call).person);
    validate(updated);

    data::save_product(tx, updated);
}

void CatalogService::archive_product(engine::Transaction& tx, const Call& call) {
    const std::string id = subject(call);

    std::optional<Product> product = data::find_product(tx, id);
    if (!product) throw RuleViolation("That product is not in the catalog.");
    if (product->archived) return; // archiving twice is not an error

    product->archived   = true;
    product->updated_at = clock_();
    product->updated_by = engine::to_string(actor(call).person);
    data::save_product(tx, *product);
}

}  // namespace squiflow::modules::catalog

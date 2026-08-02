#include "modules/catalog/domain/product.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::catalog {

void validate(const Product& product) {
    if (product.id.empty())
        throw RuleViolation("This product has no record to be saved under.");
    bool blank = true;
    for (const char c : product.name)
        if (static_cast<unsigned char>(c) > ' ') { blank = false; break; }
    if (blank)
        throw RuleViolation("A product needs a name.");
}

engine::Row to_row(const Product& product) {
    engine::Row row;
    row.set("id",          engine::Value::text(product.id));
    row.set("name",        engine::Value::text(product.name));
    row.set("description",engine::Value::text(product.description));
    row.set("archived",   engine::Value::boolean(product.archived));
    row.set("created_at", engine::Value::integer(product.created_at));
    row.set("updated_at", engine::Value::integer(product.updated_at));
    row.set("created_by", engine::Value::text(product.created_by));
    row.set("updated_by", engine::Value::text(product.updated_by));
    return row;
}

Product product_from_row(const engine::Row& row) {
    Product product;
    product.id          = row.get("id").text_or({});
    product.name        = row.get("name").text_or({});
    product.description = row.get("description").text_or({});
    product.archived    = row.get("archived").boolean_or(false);
    product.created_at  = row.get("created_at").integer_or(0);
    product.updated_at  = row.get("updated_at").integer_or(0);
    product.created_by  = row.get("created_by").text_or({});
    product.updated_by  = row.get("updated_by").text_or({});
    return product;
}

}  // namespace squiflow::modules::catalog

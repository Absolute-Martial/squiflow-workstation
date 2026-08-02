#include "modules/administration/domain/device.hpp"

#include "modules/administration/domain/person.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::administration {

void validate(const Device& device) {
    if (device.id.empty()) {
        throw RuleViolation("This device has no record to be saved under.");
    }
    if (trim(device.name).empty()) {
        // A device list of "device 1, device 2" is a device list nobody can
        // use to answer "which machine was that?".
        throw RuleViolation("Give the machine a name you would recognise.");
    }
}

engine::Row to_row(const Device& device) {
    engine::Row row;
    row.set("id", engine::Value::text(device.id));
    row.set("name", engine::Value::text(device.name));
    row.set("retired", engine::Value::boolean(device.retired));
    row.set("registered_at", engine::Value::integer(device.registered_at));
    row.set("retired_at", engine::Value::integer(device.retired_at));
    row.set("registered_by", engine::Value::text(device.registered_by));
    return row;
}

Device device_from_row(const engine::Row& row) {
    Device device;
    device.id = row.get("id").text_or({});
    device.name = row.get("name").text_or({});
    device.retired = row.get("retired").boolean_or(false);
    device.registered_at = row.get("registered_at").integer_or(0);
    device.retired_at = row.get("retired_at").integer_or(0);
    device.registered_by = row.get("registered_by").text_or({});
    return device;
}

}  // namespace squiflow::modules::administration

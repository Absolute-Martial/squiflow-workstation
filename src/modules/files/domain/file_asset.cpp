#include "modules/files/domain/file_asset.hpp"

#include <cctype>
#include <limits>

#include <squiflow/protocol/module_id.hpp>

#include "modules/context.hpp"

namespace squiflow::modules::files {
namespace {
bool blank(const std::string& s) noexcept { for(char c:s) if(static_cast<unsigned char>(c)>' ') return false; return true; }
Presence presence_from(std::int64_t v) noexcept { return v==0?Presence::Present:Presence::Missing; }
VolumeState volume_state_from(std::int64_t v) noexcept { return v==0?VolumeState::Online:(v==1?VolumeState::Offline:VolumeState::Stale); }
engine::Reference reference_from(const engine::Row& r) noexcept {
    engine::Reference x; const auto n=r.get("target_module").integer_or(-1); protocol::ModuleId m{};
    if(n<0||n>std::numeric_limits<std::uint32_t>::max()||!protocol::module_from_number(static_cast<std::uint32_t>(n),m)){x.module=protocol::ModuleId::Count;return x;}
    x.module=m;x.record=engine::record_id_from_string(r.get("target_record").text_or({}));return x;
}
}

std::optional<std::string> normalize_sha256(const std::string& value) {
    if (value.size() != 64) return std::nullopt;
    std::string out;
    out.reserve(64);
    bool any = false;
    for(char c:value){const unsigned char u=static_cast<unsigned char>(c);if(!std::isxdigit(u))return std::nullopt;char lower=static_cast<char>(std::tolower(u));if(lower!='0')any=true;out.push_back(lower);} if(!any)return std::nullopt;return out;
}
bool same_identity(const FileLocation& a,const FileLocation& b) noexcept{return a.identity==b.identity;}
void validate(const FileAsset& a){if(a.id.empty())throw RuleViolation("This file asset has no record.");auto h=normalize_sha256(a.content_hash);if(!h||*h!=a.content_hash)throw RuleViolation("A file asset needs a normalized SHA-256 hash.");if(a.size_bytes<0)throw RuleViolation("A file size cannot be negative.");if(a.predecessor_id==a.id)throw RuleViolation("A file version cannot precede itself.");if(a.created_at<=0||blank(a.created_by))throw RuleViolation("A file asset must record its creation.");if(a.forgotten){if(a.forgotten_at<=0||blank(a.forgotten_by)||blank(a.forget_reason))throw RuleViolation("Forgetting a file needs a reason and evidence.");}else if(a.forgotten_at!=0||!a.forgotten_by.empty()||!a.forget_reason.empty())throw RuleViolation("An active file cannot carry forget evidence.");}
void validate(const FileLocation& l){if(l.id.empty()||!l.identity.is_valid()||blank(l.asset_id)||blank(l.path))throw RuleViolation("A file location must identify its device, volume, file, asset, and path.");if(l.modified_at<0||l.observed_at<=0||l.scan_generation<=0)throw RuleViolation("A file location needs valid scan evidence.");}
void validate(const FileLink& l){if(l.id.empty()||blank(l.asset_id)||!l.target.is_valid()||!protocol::is_valid(l.target.module)||blank(l.role))throw RuleViolation("A file link must name its asset, target, and role.");if(l.linked_at<=0||blank(l.linked_by))throw RuleViolation("A file link must record who created it and when.");}
void validate(const FileVolume& v){if(v.id.empty()||!v.device.is_valid()||blank(v.volume_id)||v.scan_generation<0||v.observed_at<=0)throw RuleViolation("A file volume needs valid identity and scan evidence.");}
engine::Row to_row(const FileAsset&a){engine::Row r;r.set("id",engine::Value::text(a.id));r.set("content_hash",engine::Value::text(a.content_hash));r.set("size_bytes",engine::Value::integer(a.size_bytes));r.set("extension",engine::Value::text(a.extension));r.set("media_type",engine::Value::text(a.media_type));r.set("predecessor_id",engine::Value::text(a.predecessor_id));r.set("forgotten",engine::Value::boolean(a.forgotten));r.set("forgotten_at",engine::Value::integer(a.forgotten_at));r.set("forgotten_by",engine::Value::text(a.forgotten_by));r.set("forget_reason",engine::Value::text(a.forget_reason));r.set("created_at",engine::Value::integer(a.created_at));r.set("created_by",engine::Value::text(a.created_by));return r;}
engine::Row to_row(const FileLocation&l){engine::Row r;r.set("id",engine::Value::text(l.id));r.set("device_id",engine::Value::text(engine::to_string(l.identity.device)));r.set("volume_id",engine::Value::text(l.identity.volume_id));r.set("file_id",engine::Value::text(l.identity.file_id));r.set("asset_id",engine::Value::text(l.asset_id));r.set("path",engine::Value::text(l.path));r.set("presence",engine::Value::integer(static_cast<std::int64_t>(l.presence)));r.set("modified_at",engine::Value::integer(l.modified_at));r.set("observed_at",engine::Value::integer(l.observed_at));r.set("scan_generation",engine::Value::integer(l.scan_generation));return r;}
engine::Row to_row(const FileLink&l){engine::Row r;r.set("id",engine::Value::text(l.id));r.set("asset_id",engine::Value::text(l.asset_id));r.set("target_module",engine::Value::integer(static_cast<std::int64_t>(l.target.module)));r.set("target_record",engine::Value::text(engine::to_string(l.target.record)));r.set("role",engine::Value::text(l.role));r.set("search_text",engine::Value::text(l.search_text));r.set("linked_at",engine::Value::integer(l.linked_at));r.set("linked_by",engine::Value::text(l.linked_by));return r;}
engine::Row to_row(const FileVolume&v){engine::Row r;r.set("id",engine::Value::text(v.id));r.set("device_id",engine::Value::text(engine::to_string(v.device)));r.set("volume_id",engine::Value::text(v.volume_id));r.set("label",engine::Value::text(v.label));r.set("state",engine::Value::integer(static_cast<std::int64_t>(v.state)));r.set("scan_generation",engine::Value::integer(v.scan_generation));r.set("observed_at",engine::Value::integer(v.observed_at));return r;}
FileAsset asset_from_row(const engine::Row&r){FileAsset a;a.id=r.get("id").text_or({});a.content_hash=r.get("content_hash").text_or({});a.size_bytes=r.get("size_bytes").integer_or(0);a.extension=r.get("extension").text_or({});a.media_type=r.get("media_type").text_or({});a.predecessor_id=r.get("predecessor_id").text_or({});a.forgotten=r.get("forgotten").boolean_or(false);a.forgotten_at=r.get("forgotten_at").integer_or(0);a.forgotten_by=r.get("forgotten_by").text_or({});a.forget_reason=r.get("forget_reason").text_or({});a.created_at=r.get("created_at").integer_or(0);a.created_by=r.get("created_by").text_or({});return a;}
FileLocation location_from_row(const engine::Row&r){FileLocation l;l.id=r.get("id").text_or({});l.identity.device=engine::record_id_from_string(r.get("device_id").text_or({}));l.identity.volume_id=r.get("volume_id").text_or({});l.identity.file_id=r.get("file_id").text_or({});l.asset_id=r.get("asset_id").text_or({});l.path=r.get("path").text_or({});l.presence=presence_from(r.get("presence").integer_or(1));l.modified_at=r.get("modified_at").integer_or(0);l.observed_at=r.get("observed_at").integer_or(0);l.scan_generation=r.get("scan_generation").integer_or(0);return l;}
FileLink link_from_row(const engine::Row&r){FileLink l;l.id=r.get("id").text_or({});l.asset_id=r.get("asset_id").text_or({});l.target=reference_from(r);l.role=r.get("role").text_or({});l.search_text=r.get("search_text").text_or({});l.linked_at=r.get("linked_at").integer_or(0);l.linked_by=r.get("linked_by").text_or({});return l;}
FileVolume volume_from_row(const engine::Row&r){FileVolume v;v.id=r.get("id").text_or({});v.device=engine::record_id_from_string(r.get("device_id").text_or({}));v.volume_id=r.get("volume_id").text_or({});v.label=r.get("label").text_or({});v.state=volume_state_from(r.get("state").integer_or(2));v.scan_generation=r.get("scan_generation").integer_or(0);v.observed_at=r.get("observed_at").integer_or(0);return v;}
}

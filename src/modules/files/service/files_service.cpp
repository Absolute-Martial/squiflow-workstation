#include "modules/files/service/files_service.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <squiflow/protocol/module_id.hpp>

#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/files/data/repository.hpp"

namespace squiflow::modules::files {
namespace {
bool blank(const std::string&s)noexcept{for(char c:s)if(static_cast<unsigned char>(c)>' ')return false;return true;}
engine::Row fields(const Call&c){try{return engine::decode_payload(c.payload);}catch(const engine::PayloadError&){throw RuleViolation("This request could not be read. Please try it again.");}}
const engine::Session& actor(const Call&c){if(!c.actor)throw std::logic_error("files: write without session");return *c.actor;}
std::string actor_id(const Call&c){return engine::to_string(actor(c).person);}
std::string subject(const Call&c){if(blank(c.record_id))throw RuleViolation("This request does not identify its file record.");return c.record_id;}
std::string text(const engine::Row&r,const std::string&n,const char*m,bool required=false){const std::string*v=r.get(n).as_text();if(!v||(required&&blank(*v)))throw RuleViolation(m);return *v;}
std::string opt_text(const engine::Row&r,const std::string&n,const char*m,const std::string&f={}){return r.has(n)?text(r,n,m):f;}
std::int64_t number(const engine::Row&r,const std::string&n,const char*m){auto v=r.get(n).as_integer();if(!v)throw RuleViolation(m);return *v;}
std::int64_t opt_number(const engine::Row&r,const std::string&n,const char*m,std::int64_t f=0){return r.has(n)?number(r,n,m):f;}
bool opt_bool(const engine::Row&r,const std::string&n,const char*m,bool f=false){if(!r.has(n))return f;auto v=r.get(n).as_integer();if(!v||(*v!=0&&*v!=1))throw RuleViolation(m);return *v==1;}
std::string lower(std::string s){for(char&c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
engine::Reference target_from(const engine::Row&r){auto n=number(r,"target_module","That target module could not be read.");if(n<0||n>std::numeric_limits<std::uint32_t>::max())throw RuleViolation("That target module is unknown.");protocol::ModuleId m{};if(!protocol::module_from_number(static_cast<std::uint32_t>(n),m))throw RuleViolation("That target module is unknown.");engine::Reference x;x.module=m;x.record=engine::record_id_from_string(text(r,"target_record","That target record is malformed.",true));if(!x.is_valid())throw RuleViolation("That target record is malformed.");return x;}
struct Observation{FileAsset asset;FileLocation location;};
}

void FilesService::index_scan(engine::Transaction&t,const Call&c)const{
 const engine::Row r=fields(c);const std::int64_t at=clock_();const std::string device_text=text(r,"device_id","A scan must identify its device.",true);const engine::DeviceId device=engine::record_id_from_string(device_text);if(!device.is_valid())throw RuleViolation("That scan device id is malformed.");const std::string volume_id=text(r,"volume_id","A scan must identify its volume.",true);const std::string volume_record=text(r,"volume_record_id","A scan needs a volume record.",true);const std::int64_t generation=number(r,"generation","A scan needs a generation number.");if(generation<=0)throw RuleViolation("A scan generation must be positive.");const std::int64_t count=number(r,"item_count","A scan must say how many files it observed.");if(count<0||count>kMaxScanItems)throw RuleViolation("A scan may contain between 0 and 500 files.");const bool complete=opt_bool(r,"complete","That scan completion flag is malformed.");const bool online=opt_bool(r,"online","That volume state is malformed.",true);
 if(auto old=data::find_volume(t,volume_record);old&&generation<old->scan_generation)throw RuleViolation("An older scan cannot overwrite a newer one.");
 std::vector<Observation> observations;observations.reserve(static_cast<std::size_t>(count));std::set<std::string> ids;std::set<std::string> identities;
 for(std::int64_t i=0;i<count;++i){const std::string p="item."+std::to_string(i)+".";Observation o;o.location.id=text(r,p+"location_id","Every scan item needs a location record.",true);o.asset.id=text(r,p+"asset_id","Every scan item needs an asset record.",true);o.location.identity.device=device;o.location.identity.volume_id=volume_id;o.location.identity.file_id=text(r,p+"file_id","Every scan item needs a platform file id.",true);o.location.asset_id=o.asset.id;o.location.path=text(r,p+"path","Every scan item needs its current path.",true);o.location.presence=Presence::Present;o.location.modified_at=opt_number(r,p+"modified_at","That modification time is malformed.");o.location.observed_at=at;o.location.scan_generation=generation;auto hash=normalize_sha256(text(r,p+"content_hash","Every scan item needs a SHA-256 hash.",true));if(!hash)throw RuleViolation("That scan item has a malformed SHA-256 hash.");o.asset.content_hash=*hash;o.asset.size_bytes=number(r,p+"size_bytes","That file size is malformed.");o.asset.extension=lower(opt_text(r,p+"extension","That extension is malformed."));o.asset.media_type=opt_text(r,p+"media_type","That media type is malformed.");o.asset.created_at=at;o.asset.created_by=actor_id(c);validate(o.asset);validate(o.location);if(!ids.insert(o.location.id).second)throw RuleViolation("That scan lists the same location twice.");const std::string stable=device_text+"\n"+volume_id+"\n"+o.location.identity.file_id;if(!identities.insert(stable).second)throw RuleViolation("That scan lists the same stable file identity twice.");observations.push_back(std::move(o));}
 FileVolume volume;volume.id=volume_record;volume.device=device;volume.volume_id=volume_id;volume.label=opt_text(r,"volume_label","That volume label is malformed.");volume.state=online?VolumeState::Online:VolumeState::Offline;volume.scan_generation=generation;volume.observed_at=at;validate(volume);
 for(auto&o:observations){auto previous=data::find_by_identity(t,o.location.identity);auto by_hash=data::find_asset_by_hash(t,o.asset.content_hash);if(by_hash){o.asset=*by_hash;o.location.asset_id=o.asset.id;}else{if(previous&&previous->asset_id!=o.asset.id)o.asset.predecessor_id=previous->asset_id;data::save_asset(t,o.asset);}data::save_location(t,o.location);}
 if(complete&&online){engine::Query q{tables::kLocation};q.where_equals("device_id",engine::Value::text(device_text)).where_equals("volume_id",engine::Value::text(volume_id));for(const auto&row:t.select(q)){FileLocation l=location_from_row(row);if(l.scan_generation<generation){l.presence=Presence::Missing;l.observed_at=at;l.scan_generation=generation;data::save_location(t,l);}}}
 data::save_volume(t,volume);
}

std::vector<engine::Row> FilesService::search(const engine::Store&s,const Call&c)const{
 const engine::Row r=fields(c);const std::string query=lower(opt_text(r,"query","That search text is malformed."));const std::string extension=lower(opt_text(r,"extension","That extension filter is malformed."));const std::string volume=opt_text(r,"volume_id","That volume filter is malformed.");const bool present_only=opt_bool(r,"present_only","That presence filter is malformed.");const bool linked_only=opt_bool(r,"linked_only","That link filter is malformed.");const bool duplicates_only=opt_bool(r,"duplicates_only","That duplicate filter is malformed.");const std::int64_t limit=opt_number(r,"limit","That search limit is malformed.",100);if(limit<=0||limit>kMaxSearchResults)throw RuleViolation("A file search must ask for between 1 and 500 results.");engine::Query q{tables::kLocation};q.order_by("observed_at",engine::SortOrder::Descending).order_by("id");std::vector<engine::Row> out;
 for(const auto&row:s.select(q)){FileLocation l=location_from_row(row);auto a=data::find_asset(s,l.asset_id);if(!a||a->forgotten)continue;if(!extension.empty()&&a->extension!=extension)continue;if(!volume.empty()&&l.identity.volume_id!=volume)continue;if(present_only&&l.presence!=Presence::Present)continue;auto links=data::links_for_asset(s,a->id);if(linked_only&&links.empty())continue;if(duplicates_only&&data::locations_for_asset(s,a->id).size()<2)continue;std::string hay=lower(l.path+" "+a->extension+" "+a->media_type);for(const auto&link:links)hay+=" "+lower(link.search_text);if(!query.empty()&&hay.find(query)==std::string::npos)continue;engine::Row result=to_row(l);result.merge(to_row(*a));result.set("location_id",engine::Value::text(l.id));result.set("asset_id",engine::Value::text(a->id));result.set("linked",engine::Value::boolean(!links.empty()));result.set("duplicate",engine::Value::boolean(data::locations_for_asset(s,a->id).size()>1));out.push_back(std::move(result));if(out.size()>=static_cast<std::size_t>(limit))break;}
 return out;
}

void FilesService::link(engine::Transaction&t,const Call&c)const{const std::string id=subject(c);const engine::Row r=fields(c);if(data::find_link(t,id))throw RuleViolation("That file link is already on file.");FileLink l;l.id=id;l.asset_id=text(r,"asset_id","A file link must name its asset.",true);auto a=data::find_asset(t,l.asset_id);if(!a)throw RuleViolation("That file asset is not on file.");if(a->forgotten)throw RuleViolation("A forgotten file cannot receive a new link.");l.target=target_from(r);l.role=text(r,"role","A file link must name its role.",true);l.search_text=opt_text(r,"search_text","Those file search keys are malformed.");l.linked_at=clock_();l.linked_by=actor_id(c);for(const auto&old:data::links_for_asset(t,l.asset_id))if(old.target==l.target&&old.role==l.role)throw RuleViolation("That file is already linked to that record in that role.");data::save_link(t,l);}
void FilesService::forget(engine::Transaction&t,const Call&c)const{const std::string id=subject(c);const engine::Row r=fields(c);auto a=data::find_asset(t,id);if(!a)throw RuleViolation("That file asset is not on file.");if(a->forgotten)throw RuleViolation("That file asset is already forgotten.");a->forget_reason=text(r,"reason","Forgetting a file needs a reason.",true);a->forgotten=true;a->forgotten_at=clock_();a->forgotten_by=actor_id(c);data::save_asset(t,*a);}
}

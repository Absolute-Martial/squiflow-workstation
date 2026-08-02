#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "modules/files/data/tables.hpp"
#include "modules/files/domain/file_asset.hpp"

namespace squiflow::modules::files::data {
template<class R> std::optional<FileAsset> find_asset(const R&r,const std::string&id){auto x=r.find(tables::kAsset,id);return x?std::optional<FileAsset>{asset_from_row(*x)}:std::nullopt;}
template<class R> std::optional<FileLocation> find_location(const R&r,const std::string&id){auto x=r.find(tables::kLocation,id);return x?std::optional<FileLocation>{location_from_row(*x)}:std::nullopt;}
template<class R> std::optional<FileLink> find_link(const R&r,const std::string&id){auto x=r.find(tables::kLink,id);return x?std::optional<FileLink>{link_from_row(*x)}:std::nullopt;}
template<class R> std::optional<FileVolume> find_volume(const R&r,const std::string&id){auto x=r.find(tables::kVolume,id);return x?std::optional<FileVolume>{volume_from_row(*x)}:std::nullopt;}
template<class R> std::optional<FileAsset> find_asset_by_hash(const R&r,const std::string&hash){engine::Query q{tables::kAsset};q.where_equals("content_hash",engine::Value::text(hash)).order_by("id").take(1);auto rows=r.select(q);return rows.empty()?std::nullopt:std::optional<FileAsset>{asset_from_row(rows.front())};}
template<class R> std::optional<FileLocation> find_by_identity(const R&r,const engine::LocalFileIdentity&i){engine::Query q{tables::kLocation};q.where_equals("device_id",engine::Value::text(engine::to_string(i.device))).where_equals("volume_id",engine::Value::text(i.volume_id)).where_equals("file_id",engine::Value::text(i.file_id)).take(1);auto rows=r.select(q);return rows.empty()?std::nullopt:std::optional<FileLocation>{location_from_row(rows.front())};}
template<class R> std::vector<FileLocation> locations_for_asset(const R&r,const std::string&id){engine::Query q{tables::kLocation};q.where_equals("asset_id",engine::Value::text(id)).order_by("path").order_by("id");std::vector<FileLocation> out;for(auto&x:r.select(q))out.push_back(location_from_row(x));return out;}
template<class R> std::vector<FileLink> links_for_asset(const R&r,const std::string&id){engine::Query q{tables::kLink};q.where_equals("asset_id",engine::Value::text(id)).order_by("linked_at").order_by("id");std::vector<FileLink> out;for(auto&x:r.select(q))out.push_back(link_from_row(x));return out;}
template<class R> std::vector<FileLink> links_for_target(const R&r,const engine::Reference&t){engine::Query q{tables::kLink};q.where_equals("target_module",engine::Value::integer(static_cast<std::int64_t>(t.module))).where_equals("target_record",engine::Value::text(engine::to_string(t.record))).order_by("id");std::vector<FileLink> out;for(auto&x:r.select(q))out.push_back(link_from_row(x));return out;}
template<class R> std::vector<FileAsset> lineage(const R&r,const std::string&id){std::vector<FileAsset> out;std::set<std::string> seen;std::string cur=id;while(!cur.empty()&&seen.insert(cur).second){auto a=find_asset(r,cur);if(!a)break;out.push_back(*a);cur=a->predecessor_id;}return out;}
void save_asset(engine::Transaction&,const FileAsset&);void save_location(engine::Transaction&,const FileLocation&);void save_link(engine::Transaction&,const FileLink&);void save_volume(engine::Transaction&,const FileVolume&);
}

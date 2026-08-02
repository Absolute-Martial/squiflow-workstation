#include "modules/files/data/repository.hpp"

namespace squiflow::modules::files::data {
namespace {void upsert(engine::Transaction&t,const char*table,const std::string&key,const engine::Row&r){if(!t.replace(table,key,r))t.insert(table,r);}}
void save_asset(engine::Transaction&t,const FileAsset&a){validate(a);upsert(t,tables::kAsset,a.id,to_row(a));}
void save_location(engine::Transaction&t,const FileLocation&l){validate(l);upsert(t,tables::kLocation,l.id,to_row(l));}
void save_link(engine::Transaction&t,const FileLink&l){validate(l);upsert(t,tables::kLink,l.id,to_row(l));}
void save_volume(engine::Transaction&t,const FileVolume&v){validate(v);upsert(t,tables::kVolume,v.id,to_row(v));}
}

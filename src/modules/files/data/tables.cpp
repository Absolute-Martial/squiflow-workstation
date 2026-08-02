#include "modules/files/data/tables.hpp"

namespace squiflow::modules::files::tables {
std::vector<engine::Migration> migrations(){
    engine::Migration m;m.number=kFirstMigration;m.name="files tables";
    m.schema=[](engine::Store& s){s.define_table(kAsset,"id");s.define_table(kLocation,"id");s.define_table(kLink,"id");s.define_table(kVolume,"id");};
    return {m};
}
}

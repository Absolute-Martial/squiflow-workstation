#include "modules/files/module.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "modules/files/data/tables.hpp"
#include "modules/files/service/files_service.hpp"
#include "modules/registry.hpp"

namespace squiflow::modules::files {
namespace {
class FilesModule final:public Module{
public:explicit FilesModule(std::function<std::int64_t()> clock):service_(std::move(clock)){}
 protocol::ModuleId id()const noexcept override{return protocol::ModuleId::files;}
 std::vector<engine::Migration> migrations()const override{return tables::migrations();}
 void install(Registry&r)override{
  r.on_write(protocol::OperationId::file_index_scan,[this](engine::Transaction&t,const Call&c){service_.index_scan(t,c);});
  r.on_read(protocol::OperationId::file_search,[this](const engine::Store&s,const Call&c){return service_.search(s,c);});
  r.on_write(protocol::OperationId::file_link,[this](engine::Transaction&t,const Call&c){service_.link(t,c);});
  r.on_write(protocol::OperationId::file_forget,[this](engine::Transaction&t,const Call&c){service_.forget(t,c);});
 }
private:FilesService service_;
};
}
ModulePtr make_module(std::function<std::int64_t()> clock){if(!clock)throw std::logic_error("files needs a clock");return std::make_unique<FilesModule>(std::move(clock));}
}

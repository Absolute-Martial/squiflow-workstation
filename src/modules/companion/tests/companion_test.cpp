#include <atomic>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/companion/data/repository.hpp"
#include "modules/companion/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace companion = squiflow::modules::companion;
namespace protocol = squiflow::protocol;

namespace {
std::atomic<std::int64_t> g_now{1'800'000'000'000};
std::int64_t now(){return g_now.fetch_add(1000)+1000;}
std::atomic<int> g_key{0};
std::string key(){return "cmp-key-"+std::to_string(g_key.fetch_add(1)+1);}
const std::string kPerson="91000000000000000000000000000001";
const std::string kTaskA="92000000000000000000000000000001";
const std::string kTaskB="92000000000000000000000000000002";
const std::string kTaskC="92000000000000000000000000000003";
const std::string kTarget="93000000000000000000000000000001";

engine::Blob payload(std::initializer_list<std::pair<std::string,std::string>> texts={},
                     std::initializer_list<std::pair<std::string,std::int64_t>> numbers={}){
    engine::Row row;
    for(const auto& [n,v]:texts) row.set(n,engine::Value::text(v));
    for(const auto& [n,v]:numbers) row.set(n,engine::Value::integer(v));
    return engine::encode_payload(row);
}
engine::Session owner(){engine::Session s;s.person=engine::record_id_from_string(kPerson);s.device={1,1};s.is_owner=true;s.rights.grant_all();return s;}
engine::Session staff(std::initializer_list<protocol::RightId> rights){engine::Session s;s.person=engine::record_id_from_string(kPerson);s.device={1,2};for(auto r:rights)s.rights.grant(r);return s;}
template<class Fn> bool violates(Fn&& fn){try{fn();}catch(const modules::RuleViolation&){return true;}return false;}

struct Shop{
    modules::Registry registry{now};std::unique_ptr<engine::Database> database;
    Shop(){registry.add(companion::make_module(now));engine::MigrationRunner runner{now};registry.collect_migrations(runner);database=std::make_unique<engine::Database>(std::make_unique<engine::MemoryStore>(),std::move(runner));database->open();}
    modules::Outcome run(protocol::OperationId op,const std::string& id,const engine::Blob& body,const engine::Session& session,engine::ConnectionState connection=engine::ConnectionState::Online,bool with_key=true){modules::Call call;call.operation=op;call.record_id=id;call.payload=body;if(protocol::operation(op).sync_class==protocol::OperationClass::Synchronizable)call.idempotency_key=with_key?key():std::string{};return registry.run(*database,call,session,connection);}
    template<class Fn> void read(Fn&& fn)const{database->read([&](const engine::Store& store){fn(store);});}
};

std::int64_t module_number(protocol::ModuleId id){return static_cast<std::int64_t>(id);}
engine::Blob reminder(const std::string& title="Call customer",std::int64_t due=1'800'000'100'000){return payload({{"kind","reminder"},{"title",title},{"target_record",kTarget}},{{"target_module",module_number(protocol::ModuleId::parties)},{"due_at",due}});}
}

int main(){
 const engine::Session session=owner();
 section("migration 20 and exact companion operation surface");{
  Shop shop;check(shop.registry.handled(protocol::OperationId::task_create),"create handled");check(shop.registry.handled(protocol::OperationId::task_update),"update handled");check(shop.registry.handled(protocol::OperationId::task_complete),"complete handled");check(shop.registry.handled(protocol::OperationId::task_snooze),"snooze handled");check(!shop.registry.handled(protocol::OperationId::purchase_lookup),"nothing else claimed");check(companion::tables::kFirstMigration==20,"migration 20");
  for(auto op:{protocol::OperationId::task_create,protocol::OperationId::task_update,protocol::OperationId::task_complete,protocol::OperationId::task_snooze}){const auto& info=protocol::operation(op);check(info.module==protocol::ModuleId::companion,"operation belongs to companion");check(info.right==protocol::RightId::right_task_write,"write right exact");check(info.sync_class==protocol::OperationClass::Synchronizable,"operation synchronizes");check(info.offline==protocol::OfflineRule::OfflineAllowed,"operation is offline allowed");}
  shop.read([](const engine::Store& s){check(s.has_table(companion::tables::kTask),"task table");check(s.has_table(companion::tables::kEvent),"event table");check(s.count(companion::tables::kTask)==0,"no tasks initially");check(s.count(companion::tables::kEvent)==0,"no events initially");});
 }
 section("calendar recurrence is strict and clamps real month ends");{
  using namespace std::chrono;const auto ms=[](year_month_day y){return duration_cast<milliseconds>(sys_days{y}.time_since_epoch()).count();};
  check(companion::next_due_after(ms(2028y/January/31),companion::RecurrenceUnit::Month,1)==ms(2028y/February/29),"leap February clamp");
  check(companion::next_due_after(ms(2027y/January/31),companion::RecurrenceUnit::Month,1)==ms(2027y/February/28),"ordinary February clamp");
  check(companion::next_due_after(ms(2028y/February/29),companion::RecurrenceUnit::Year,1)==ms(2029y/February/28),"leap day yearly clamp");
  check(companion::next_due_after(ms(2028y/March/31),companion::RecurrenceUnit::Month,2)==ms(2028y/May/31),"two-month interval");
  check(companion::next_due_after(1000,companion::RecurrenceUnit::Day,1)==86'401'000,"one day exact");check(companion::next_due_after(1000,companion::RecurrenceUnit::Week,2)==1'209'601'000,"two weeks exact");
  check(!companion::next_due_after(0,companion::RecurrenceUnit::Day,1),"zero anchor refused");check(!companion::next_due_after(1000,companion::RecurrenceUnit::None,1),"no unit refused");check(!companion::next_due_after(1000,companion::RecurrenceUnit::Day,0),"zero interval refused");check(!companion::next_due_after(1000,companion::RecurrenceUnit::Day,-1),"negative interval refused");check(!companion::next_due_after(std::numeric_limits<std::int64_t>::max()-1,companion::RecurrenceUnit::Day,1),"overflow refused");
  companion::Task t;t.id=kTaskA;t.title="Plain";t.created_at=1;t.created_by=kPerson;t.updated_at=1;t.updated_by=kPerson;check(!violates([&]{companion::validate(t);}),"minimal personal task valid");t.title=" \t";check(violates([&]{companion::validate(t);}),"blank title refused");
 }
 section("tasks attach generically, update, snooze, return and complete with evidence");{
  Shop shop;const auto made=shop.run(protocol::OperationId::task_create,kTaskA,reminder(),session,engine::ConnectionState::Offline);check(made.ok&&made.queued,"offline reminder queues");
  shop.read([](const engine::Store& s){auto t=companion::data::find_task(s,kTaskA);check(t.has_value(),"task saved");check(t->kind==companion::TaskKind::Reminder,"kind saved");check(t->target.module==protocol::ModuleId::parties,"module saved");check(engine::to_string(t->target.record)==kTarget,"record saved");check(companion::data::events_for_task(s,kTaskA).size()==1,"creation event saved");check(companion::data::tasks_for_target(s,t->target).size()==1,"generic target query works");});
  check(shop.run(protocol::OperationId::task_update,kTaskA,payload({{"title","Call accounts"},{"note","Ask for PO"}}),session).ok,"open task updates");
  const std::int64_t until=g_now.load()+500'000;check(shop.run(protocol::OperationId::task_snooze,kTaskA,payload({{"reason","Customer travelling"}},{{"snoozed_until",until}}),session).ok,"future snooze works");
  shop.read([&](const engine::Store& s){auto t=companion::data::find_task(s,kTaskA);check(t->title=="Call accounts","title updated");check(t->due_at==1'800'000'100'000,"original due preserved");check(t->snoozed_until==until,"return time saved");check(!companion::visible_at(*t,until-1),"hidden before return");check(companion::visible_at(*t,until),"visible exactly on return");check(companion::data::events_for_task(s,kTaskA).size()==3,"create update snooze events retained");});
  check(!shop.run(protocol::OperationId::task_snooze,kTaskA,payload({{"reason",""}},{{"snoozed_until",until+1}}),session).ok,"blank snooze reason refused");check(!shop.run(protocol::OperationId::task_snooze,kTaskA,payload({{"reason","past"}},{{"snoozed_until",1}}),session).ok,"past snooze refused");
  check(shop.run(protocol::OperationId::task_complete,kTaskA,payload({}),session).ok,"task completes");check(!shop.run(protocol::OperationId::task_complete,kTaskA,payload({}),session).ok,"double completion refused");check(!shop.run(protocol::OperationId::task_update,kTaskA,payload({{"title","rewrite"}}),session).ok,"completed update refused");check(!shop.run(protocol::OperationId::task_snooze,kTaskA,payload({{"reason","later"}},{{"snoozed_until",until+1000}}),session).ok,"completed snooze refused");
  shop.read([](const engine::Store& s){auto t=companion::data::find_task(s,kTaskA);check(t->state==companion::TaskState::Completed,"final state");check(t->completed_at>0&&!t->completed_by.empty(),"completion evidence");check(t->snoozed_until==0,"completion clears snooze");check(companion::data::events_for_task(s,kTaskA).size()==4,"completion event appended");check(companion::data::visible_tasks(s,std::numeric_limits<std::int64_t>::max()).empty(),"completed absent from visible");});
 }
 section("recurring completion advances while deterministic attention never duplicates");{
  Shop shop;const std::int64_t due=1'800'100'000'000;auto recurring=payload({{"kind","recurring"},{"title","Monthly statements"},{"recurrence_unit","month"}},{{"due_at",due},{"recurrence_interval",1}});check(shop.run(protocol::OperationId::task_create,kTaskA,recurring,session).ok,"recurring task created");check(shop.run(protocol::OperationId::task_complete,kTaskA,payload({}),session).ok,"occurrence completed");
  shop.read([&](const engine::Store& s){auto t=companion::data::find_task(s,kTaskA);check(t->state==companion::TaskState::Open,"recurring remains open");check(t->due_at>due,"next due advanced");check(t->completion_count==1,"occurrence counted");check(t->last_completed_at>0&&!t->last_completed_by.empty(),"latest evidence retained");check(companion::data::events_for_task(s,kTaskA).size()==2,"completion history retained");check(companion::data::recurring_tasks(s).size()==1,"recurring query finds it");});
  auto attention=payload({{"kind","attention"},{"title","Invoice overdue"},{"source_key","invoice-aged:abc:stage-1"},{"target_record",kTarget}},{{"target_module",module_number(protocol::ModuleId::receivables)},{"due_at",due}});check(shop.run(protocol::OperationId::task_create,kTaskB,attention,session).ok,"attention created");check(!shop.run(protocol::OperationId::task_create,kTaskC,attention,session).ok,"duplicate source refused");shop.read([](const engine::Store& s){check(s.count(companion::tables::kTask)==2,"duplicate left no task");check(s.count(companion::tables::kEvent)==3,"duplicate left no event");check(companion::data::find_by_source_key(s,"invoice-aged:abc:stage-1")->id==kTaskB,"source lookup exact");});
 }
 section("bad references rights malformed payloads and replay fail safely");{
  Shop shop;check(!shop.run(protocol::OperationId::task_create,kTaskA,payload({{"title","bad"},{"target_record","broken"}},{{"target_module",999}}),session).ok,"unknown module refused");check(!shop.run(protocol::OperationId::task_create,kTaskA,payload({{"kind","reminder"},{"title","half"},{"target_record",kTarget}},{{"due_at",100}}),session).ok,"half target refused");check(!shop.run(protocol::OperationId::task_create,kTaskA,payload({{"kind","reminder"},{"title","bad id"},{"target_record","no"}},{{"target_module",module_number(protocol::ModuleId::jobs)},{"due_at",100}}),session).ok,"malformed record refused");
  engine::Session nobody;nobody.rights.grant_all();auto denied=shop.run(protocol::OperationId::task_create,kTaskA,payload({{"title","x"}}),nobody);check(!denied.ok&&denied.reason==engine::DenialReason::NotSignedIn,"authentication first");auto reader=staff({protocol::RightId::right_task_read});check(!shop.run(protocol::OperationId::task_create,kTaskA,payload({{"title","x"}}),reader).ok,"read right not write");auto writer=staff({protocol::RightId::right_task_write});check(shop.run(protocol::OperationId::task_create,kTaskA,payload({{"title","x"}}),writer).ok,"write right works");auto offline=shop.run(protocol::OperationId::task_update,kTaskA,payload({{"note","offline"}}),writer,engine::ConnectionState::Offline);check(!offline.ok&&offline.reason==engine::DenialReason::ReadOnlyOffline,"staff offline guard remains");
  auto malformed=shop.run(protocol::OperationId::task_create,kTaskB,engine::Blob{1,2,3},session);check(!malformed.ok&&malformed.error=="This request could not be read. Please try it again.","malformed standard refusal");bool loud=false;try{shop.run(protocol::OperationId::task_create,"",payload({{"title","x"}}),session);}catch(const modules::RegistryError&){loud=true;}check(loud,"missing record loud");
  modules::Call call;call.operation=protocol::OperationId::task_create;call.record_id=kTaskB;call.payload=payload({{"title","Replay"}});call.idempotency_key="same-companion-key";auto first=shop.registry.run(*shop.database,call,session,engine::ConnectionState::Online);auto replay=shop.registry.run(*shop.database,call,session,engine::ConnectionState::Online);check(first.ok&&!first.replayed,"first runs");check(replay.ok&&replay.replayed&&!replay.queued,"replay harmless");shop.read([](const engine::Store& s){check(s.count(companion::tables::kTask)==2,"no duplicate task");check(companion::data::events_for_task(s,kTaskB).size()==1,"no duplicate event");});
 }
 return squiflow::testing::report();
}

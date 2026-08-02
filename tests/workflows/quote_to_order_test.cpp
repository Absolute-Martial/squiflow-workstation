#include <cstdint>
#include <memory>
#include <string>

#include "engine/audit/audit_log.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/sync/outbox.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/orders/module.hpp"
#include "modules/pricing/module.hpp"
#include "modules/quotations/data/repository.hpp"
#include "modules/quotations/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"
#include "workflows/quote_to_order.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace e=squiflow::engine; namespace m=squiflow::modules;
namespace o=squiflow::modules::orders; namespace q=squiflow::modules::quotations;
namespace p=squiflow::protocol; namespace w=squiflow::workflows;
namespace {
std::int64_t now(){return 1700000000000LL;}
const std::string Q="51000000000000000000000000000001", R="51000000000000000000000000000002", L="51000000000000000000000000000003", O="52000000000000000000000000000001", O2="52000000000000000000000000000002", PARTY="53000000000000000000000000000001";
e::Session session(){e::Session s;s.person=e::record_id_from_string("54000000000000000000000000000001");s.device=e::record_id_from_string("54000000000000000000000000000002");s.rights.grant_all();return s;}
e::Blob payload(std::int64_t revision=1){e::Row r;r.set("quotation_id",e::Value::text(Q));r.set("revision",e::Value::integer(revision));r.set("promised_at",e::Value::integer(1700000100000LL));r.set("note",e::Value::text("Keep exact snapshot"));return e::encode_payload(r);}
struct Shop{
 m::Registry registry{now}; std::unique_ptr<e::Database> db;
 Shop(){registry.add(squiflow::modules::pricing::make_module(now));registry.add(o::make_module(now));registry.add(q::make_module(now));registry.install_workflow(w::make_quote_to_order(now));e::MigrationRunner runner{now};registry.collect_migrations(runner);db=std::make_unique<e::Database>(std::make_unique<e::MemoryStore>(),std::move(runner));db->open();}
 void seed(bool accepted=true){db->write([&](e::Transaction& t){q::Quotation h;h.id=Q;h.party_id=PARTY;h.state=accepted?q::QuotationState::Accepted:q::QuotationState::Issued;h.current_revision=1;h.accepted_revision=accepted?1:0;h.created_at=now();h.created_by="maker";if(accepted){h.accepted_at=now();h.accepted_by="buyer";}q::data::save_quotation(t,h);q::QuotationRevision r;r.id=R;r.quotation_id=Q;r.revision=1;r.issued=true;r.series="Q";r.number=1;r.total_minor=2500;r.created_at=now();r.created_by="maker";r.issued_at=now();r.issued_by="maker";q::data::save_revision(t,r);q::QuotationLine l;l.id=L;l.revision_id=R;l.quotation_id=Q;l.product_id="55000000000000000000000000000001";l.description="Unicode " "\xC3\xA9";l.quantity_scaled=1000;l.unit_price_minor=2500;l.amount_minor=2500;l.rate_origin=e::RateOrigin::CatalogDefault;q::data::save_line(t,l);});}
 m::Outcome run(const std::string& order,const std::string& key,std::int64_t rev=1){m::Call c;c.operation=p::OperationId::quote_to_order;c.record_id=order;c.idempotency_key=key;c.payload=payload(rev);return registry.run(*db,c,session(),e::ConnectionState::Online);}
};
}
int main(){
 section("one accepted revision becomes one immutable order snapshot");{Shop s;s.seed();auto result=s.run(O,"q2o-1");check(result.ok&&result.queued,"conversion succeeds and queues once");s.db->read([&](const e::Store& store){auto order=o::data::find_order(store,O);check(order&&order->source_quotation_id==Q,"order retains quotation evidence");check(order&&order->source_revision_id==R&&order->source_revision==1,"order pins exact revision");check(order&&order->party_id==PARTY&&order->note=="Keep exact snapshot","customer and optional fields copy");auto lines=o::data::lines_for_order(store,O);check(lines.size()==1,"all lines copy once");check(lines[0].description=="Unicode " "\xC3\xA9"&&lines[0].unit_price_minor==2500,"text and frozen price copy exactly");check(o::data::total_for_order(store,O).value.minor==2500,"order and quotation totals agree");check(store.find(e::Outbox::table_name(),"q2o-1").has_value(),"one outbox row exists");check(store.find(e::AuditLog::table_name(),"q2o-1").has_value(),"one audit row exists");});auto replay=s.run(O,"q2o-1");check(replay.ok&&replay.replayed,"same key replays before handler");auto duplicate=s.run(O2,"q2o-2");check(!duplicate.ok,"different key cannot convert accepted revision twice");}
 section("wrong state and wrong revision refuse without partial writes");{Shop s;s.seed(false);check(!s.run(O,"bad-state").ok,"issued but unaccepted quotation is refused");s.db->read([&](const e::Store& store){check(!o::data::find_order(store,O),"state refusal writes no order");check(!store.find(e::Outbox::table_name(),"bad-state"),"state refusal writes no outbox");});}{Shop s;s.seed();check(!s.run(O,"bad-revision",2).ok,"non-accepted revision is refused");s.db->read([&](const e::Store& store){check(!o::data::find_order(store,O),"revision refusal rolls back");});}
 section("a line collision rolls back the order audit and outbox");{Shop s;s.seed();s.db->write([&](e::Transaction& t){o::Order existing;existing.id=O2;existing.created_at=now();existing.created_by="maker";o::data::save_order(t,existing);o::OrderLine line;line.id=L;line.order_id=O2;line.description="existing";line.quantity_scaled=1000;line.unit_price_minor=1;line.price_source=squiflow::modules::pricing::RateSource::Default;line.added_at=now();line.added_by="maker";o::data::save_line(t,line);});auto result=s.run(O,"collision");check(!result.ok,"collision after order write is refused");s.db->read([&](const e::Store& store){check(!o::data::find_order(store,O),"new order rolled back");check(!store.find(e::Outbox::table_name(),"collision"),"outbox rolled back");check(!store.find(e::AuditLog::table_name(),"collision"),"audit rolled back");check(o::data::find_order(store,O2).has_value(),"pre-existing data survives rollback");});}
 return squiflow::testing::report();
}

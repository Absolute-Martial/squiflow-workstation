#include "workflows/apply_agreement.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/agreements/data/repository.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/registry.hpp"

namespace squiflow::workflows { namespace {
bool blank(const std::string& s) noexcept { return std::all_of(s.begin(),s.end(),[](char c){return static_cast<unsigned char>(c)<=static_cast<unsigned char>(' ');}); }
engine::Row fields(const modules::Call& c){try{return engine::decode_payload(c.payload);}catch(const engine::PayloadError&){throw modules::RuleViolation("This agreement application could not be read.");}}
std::string text(const engine::Row&r,const char*k,const char*m){auto*p=r.get(k).as_text();if(!p||blank(*p))throw modules::RuleViolation(m);return *p;}
std::int64_t positive(const engine::Row&r,const char*k,const char*m){auto v=r.get(k).as_integer();if(!v||*v<=0)throw modules::RuleViolation(m);return *v;}
void reject(const engine::Row&r){static const std::set<std::string>a{"invoice_id","invoice_line_id","agreement_id","agreement_line_id","expected_quantity_scaled","expected_current_rate_minor"};for(const auto&f:r.fields())if(!a.contains(f.first))throw modules::RuleViolation("Unknown agreement-application field: "+f.first+".");}
WorkflowResult apply(engine::Transaction&t,const modules::Call&c,const ApplyAgreementClock&clock){
 const engine::Row r=fields(c);reject(r);if(!engine::record_id_from_string(c.record_id).is_valid())throw modules::RuleViolation("The agreement application identity is invalid.");
 const std::string invoice_id=text(r,"invoice_id","An invoice is required.");const std::string line_id=text(r,"invoice_line_id","An invoice line is required.");const std::string agreement_id=text(r,"agreement_id","An agreement is required.");const std::string agreement_line_id=text(r,"agreement_line_id","An exact agreement line is required.");
 if(!engine::record_id_from_string(invoice_id).is_valid()||!engine::record_id_from_string(line_id).is_valid()||!engine::record_id_from_string(agreement_id).is_valid()||!engine::record_id_from_string(agreement_line_id).is_valid())throw modules::RuleViolation("An agreement application contains an invalid identity.");
 auto invoice=modules::receivables::data::find_invoice(t,invoice_id);if(!invoice)throw modules::RuleViolation("That invoice is not on file.");modules::receivables::validate(*invoice);if(invoice->state!=engine::DocumentState::Draft)throw modules::RuleViolation("Agreement rates can only be applied to invoice drafts.");
 auto target=modules::receivables::data::find_invoice_line(t,line_id);if(!target||target->invoice_id!=invoice_id)throw modules::RuleViolation("That invoice line is not on this draft.");modules::receivables::validate(*target);
 const std::int64_t expected_quantity = positive(
     r, "expected_quantity_scaled", "The confirmed quantity must be positive.");
 if (target->quantity_scaled != expected_quantity) {
     throw modules::RuleViolation("The invoice quantity changed after confirmation.");
 }
 const auto expected = r.get("expected_current_rate_minor").as_integer();
 if (!expected || *expected < 0 || target->rate_minor != *expected) {
     throw modules::RuleViolation("The invoice rate changed after confirmation.");
 }
 auto agreement=modules::agreements::data::find_agreement(t,agreement_id);if(!agreement)throw modules::RuleViolation("That agreement is not on file.");modules::agreements::validate(*agreement);const std::int64_t at=clock();if(at<=0)throw modules::RuleViolation("The agreement application time is invalid.");if(agreement->state!=modules::agreements::AgreementState::Open||agreement->valid_from>at||modules::agreements::lapsed_at_moment(*agreement,at))throw modules::RuleViolation("That agreement is not currently in force.");if(agreement->party_id!=invoice->party_id)throw modules::RuleViolation("That agreement belongs to another customer.");
 auto source=modules::agreements::data::find_line(t,agreement_line_id);if(!source||source->agreement_id!=agreement_id)throw modules::RuleViolation("That rate is not on the selected agreement.");modules::agreements::validate(*source);if(source->product_id!=target->product_id)throw modules::RuleViolation("The selected agreement rate prices another product.");
 target->rate_minor=source->rate_minor;target->rate_origin=engine::RateOrigin::Agreement;target->rate_reason="Agreement "+agreement_id+": "+source->agreed_name;target->agreement_id=agreement_id;target->agreement_line_id=agreement_line_id;target->agreement_rate_minor=source->rate_minor;target->agreement_quantity_scaled=target->quantity_scaled;auto amount=modules::receivables::calculate_amount(*target);if(!amount.ok)throw modules::RuleViolation("The agreed amount is too large to store.");target->amount_minor=amount.value.minor;modules::receivables::data::save_invoice_line(t,*target);
 return {{protocol::ModuleId::receivables,engine::record_id_from_string(line_id)},"Applied agreement rate to invoice draft line without consuming its cap."};
}
}
WorkflowDefinition make_apply_agreement(ApplyAgreementClock clock){if(!clock)throw modules::RegistryError("apply_agreement needs a clock");return {protocol::OperationId::apply_agreement,{protocol::ModuleId::agreements,protocol::ModuleId::pricing,protocol::ModuleId::receivables},[clock=std::move(clock)](engine::Transaction&t,const modules::Call&c){return apply(t,c,clock);}};}
}  // namespace squiflow::workflows

#include "workflows/registration.hpp"

#include "modules/registry.hpp"
#include "workflows/apply_agreement.hpp"
#include "workflows/cancel_and_reissue.hpp"
#include "workflows/counter_sale.hpp"
#include "workflows/document_delivery.hpp"
#include "workflows/issue_invoice.hpp"
#include "workflows/order_to_jobs.hpp"
#include "workflows/quote_to_order.hpp"
#include "workflows/record_purchase.hpp"
#include "workflows/take_payment.hpp"

namespace squiflow::workflows {

void register_all_workflows(modules::Registry& registry,
                            RegistrationClock clock) {
    if (!clock) {
        throw modules::RegistryError("workflow registration needs a clock");
    }
    registry.install_workflow(make_quote_to_order(clock));
    registry.install_workflow(make_order_to_jobs(clock));
    registry.install_workflow(make_issue_invoice(clock));
    registry.install_workflow(make_cancel_and_reissue(clock));
    registry.install_workflow(make_apply_agreement(clock));
    registry.install_workflow(make_take_payment(clock));
    registry.install_workflow(make_counter_sale(clock));
    registry.install_workflow(make_record_purchase(clock));
    registry.install_workflow(make_prepare_document_delivery(clock));
    registry.install_workflow(make_request_document_delivery(clock));
}

}  // namespace squiflow::workflows

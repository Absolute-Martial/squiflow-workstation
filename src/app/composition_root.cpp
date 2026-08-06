#include "app/composition_root.hpp"
#include "modules/administration/module.hpp"
#include "modules/agreements/module.hpp"
#include "modules/catalog/module.hpp"
#include "modules/companion/module.hpp"
#include "modules/files/module.hpp"
#include "modules/jobs/module.hpp"
#include "modules/orders/module.hpp"
#include "modules/parties/module.hpp"
#include "modules/pricing/module.hpp"
#include "modules/quotations/module.hpp"
#include "modules/receivables/module.hpp"
#include "modules/sourcing/module.hpp"
#include "workflows/registration.hpp"
namespace squiflow::app {
void register_all_modules(modules::Registry& r,std::function<std::int64_t()> c){
 r.add(modules::administration::make_module(c));r.add(modules::parties::make_module(c));r.add(modules::catalog::make_module(c));r.add(modules::pricing::make_module(c));r.add(modules::quotations::make_module(c));r.add(modules::orders::make_module(c));r.add(modules::jobs::make_module(c));r.add(modules::receivables::make_module(c));r.add(modules::agreements::make_module(c));r.add(modules::sourcing::make_module(c));r.add(modules::companion::make_module(c));r.add(modules::files::make_module(c));
 workflows::register_all_workflows(r,std::move(c));
 r.require_complete();}
}

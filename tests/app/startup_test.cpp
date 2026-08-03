#include "app/activation_controller.hpp"
#include "app/application.hpp"
#include "app/composition_root.hpp"
#include "app/qt_message_mapping.hpp"
#include "support/check.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
using namespace squiflow::app;
namespace {
struct Runtime final:StartupRuntime{
 std::vector<StartupStep> started,stopped;int fail_at=-1;int secondary_at=-1;int throw_stop=-1;StartupSequence* sequence=nullptr;
 StepResult start(StartupStep s)override{started.push_back(s);const int i=static_cast<int>(started.size())-1;if(i==secondary_at)return{StepDisposition::SecondaryInstance,""};if(i==fail_at)return{StepDisposition::Failed,"failed"};if(i==4&&sequence)sequence->shutdown(ShutdownReason::WindowClosed);return{};}
 void stop(StartupStep s,ShutdownReason)override{stopped.push_back(s);if(static_cast<int>(s)==throw_stop)throw std::runtime_error("rollback failed");}
};
struct Surface final:ApplicationSurface{int shown=0,activated=0,closed=0;void show()override{++shown;}void activate()override{++activated;}void close()override{++closed;}};
}
int main(){namespace test=squiflow::testing;
test::section("fixed startup and reverse shutdown");Runtime ok;StartupSequence seq(ok);auto result=seq.start();test::check(result.disposition==StartupDisposition::Running,"startup runs");test::check(ok.started.size()==12,"all steps run");test::check(std::equal(ok.started.begin(),ok.started.end(),startup_order().begin()),"fixed order");seq.shutdown(ShutdownReason::NormalExit);test::check(ok.stopped.size()==12,"all steps stop");test::check(ok.stopped.front()==StartupStep::Window&&ok.stopped.back()==StartupStep::Paths,"reverse order");seq.shutdown(ShutdownReason::NormalExit);test::check(ok.stopped.size()==12,"shutdown idempotent");
test::section("failure matrix and exception-contained rollback");for(int fail=0;fail<12;++fail){Runtime rt;rt.fail_at=fail;rt.throw_stop=fail>1?fail-2:-1;StartupSequence s(rt);const auto r=s.start();test::check(r.disposition==StartupDisposition::Failed,"injected step fails");test::check(rt.stopped.size()==static_cast<std::size_t>(fail),"completed steps unwind");if(fail>1)test::check(s.rollback_failures().size()==1,"rollback exception captured");}
test::section("secondary never reaches database");Runtime secondary;secondary.secondary_at=3;StartupSequence s2(secondary);auto sr=s2.start();test::check(sr.disposition==StartupDisposition::SecondaryInstance,"secondary disposition");test::check(secondary.started.size()==4,"secondary stops before database");test::check(secondary.stopped.size()==3,"early resources unwind");
test::section("reentrant shutdown");Runtime reentrant;StartupSequence s3(reentrant);reentrant.sequence=&s3;auto rr=s3.start();test::check(rr.disposition==StartupDisposition::Failed,"reentrant shutdown interrupts startup");test::check(reentrant.started.size()==5,"shutdown observed after callback");test::check(reentrant.stopped.size()==5,"reentrant shutdown unwinds");
test::section("activation coalescing");ActivationController activation;Surface surface;activation.request();activation.request();test::check(activation.pending(),"pre-surface activation pending");activation.attach(surface);test::check(surface.activated==1,"pending burst coalesced");activation.request();test::check(surface.activated==2,"live activation delivered");activation.stop();activation.request();test::check(surface.activated==2,"stopped activation ignored");
test::section("Qt mapping and recursion guard");test::check(map_qt_message(QtMessageKind::Fatal)==squiflow::platform::LogLevel::Fatal,"fatal maps");QtMessageRecursionGuard outer;QtMessageRecursionGuard inner;test::check(outer.entered()&&!inner.entered(),"recursive bridge suppressed");
test::section("composition manifest");test::check(kCompositionModules.size()==12,"twelve modules");auto names=kCompositionModules;std::sort(names.begin(),names.end());test::check(std::adjacent_find(names.begin(),names.end())==names.end(),"module names unique");return test::report();}

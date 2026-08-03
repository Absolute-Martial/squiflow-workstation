#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace squiflow::app {
inline constexpr std::size_t kStartupStepCount=12;
inline constexpr std::size_t kMaxLifecycleDiagnostic=512;
enum class StartupStep { Paths, Logging, CrashHandler, SingleInstance, DatabaseConnection, Migrations, IntegrityCheck, IdentitySession, Activation, ModuleRegistration, Shell, Window };
enum class StepDisposition { Started, SecondaryInstance, Failed };
enum class StartupDisposition { Running, SecondaryInstance, Failed };
enum class ShutdownReason { NormalExit, WindowClosed, SessionEnding, SystemShutdown, StartupFailure, FatalApplicationError };
enum class LifecycleState { Idle, Starting, Running, Stopping, Stopped, Failed, SecondaryInstance };
struct StepResult { StepDisposition disposition{StepDisposition::Started}; std::string message; };
struct StartupFailure { StartupStep step; std::string message; };
struct StartupResult { StartupDisposition disposition{StartupDisposition::Failed}; std::optional<StartupFailure> failure; };
struct RollbackFailure { StartupStep step; std::string message; };
constexpr std::array<StartupStep,kStartupStepCount> startup_order(){return {StartupStep::Paths,StartupStep::Logging,StartupStep::CrashHandler,StartupStep::SingleInstance,StartupStep::DatabaseConnection,StartupStep::Migrations,StartupStep::IntegrityCheck,StartupStep::IdentitySession,StartupStep::Activation,StartupStep::ModuleRegistration,StartupStep::Shell,StartupStep::Window};}
std::string_view startup_step_name(StartupStep step) noexcept;
std::string bounded_lifecycle_message(std::string_view message);
class StartupRuntime { public: virtual ~StartupRuntime()=default; virtual StepResult start(StartupStep step)=0; virtual void stop(StartupStep step,ShutdownReason reason)=0; virtual void rollback_diagnostic(const RollbackFailure&) noexcept {} };
}
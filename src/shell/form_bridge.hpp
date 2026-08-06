#pragma once
#include "app/contracts/request_context.hpp"
#include "app/contracts/result.hpp"
#include "shell/screen_registry.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace squiflow::shell {
enum class FormFieldKind:std::uint8_t{Text,Integer,MoneyMinor,Date,Boolean};
struct FormFieldSpec{std::string id,label_key;FormFieldKind kind{FormFieldKind::Text};bool required{};std::int64_t minimum{};std::int64_t maximum{};bool has_range{};};
struct FormValue{std::string id,value;};
struct FormFieldError{std::string field,message_key;};
struct FormRequest{std::uint64_t generation{};app::RequestContext context;std::vector<FormValue> values;};
enum class FormErrorCode:std::uint8_t{InvalidField,Validation,Pending,NoPending,Stale};
struct FormError{FormErrorCode code;std::string message_key;std::optional<std::string> field;};
struct PresentedDomainError{std::string message_key;std::optional<std::string> field;bool retryable{};};
std::string domain_error_message(app::DomainErrorCode code);
PresentedDomainError present_domain_error(const app::DomainError& error);
class FormBridge final:public PresentationBridge{
public:FormBridge(std::vector<FormFieldSpec>,app::RequestContext);
app::Result<void,FormError> set(std::string_view,std::string);
app::Result<FormRequest,FormError> begin_submit();
app::Result<void,FormError> complete(std::uint64_t,app::Result<std::string,app::DomainError>);
void cancel() noexcept;bool pending()const noexcept{return pending_;}bool dirty()const noexcept{return dirty_;}
const auto& field_errors()const noexcept{return errors_;}const auto& form_error()const noexcept{return form_error_;}const auto& saved_id()const noexcept{return saved_id_;}
private:const FormFieldSpec* spec(std::string_view)const noexcept;static std::optional<std::int64_t> integer(std::string_view)noexcept;static std::optional<std::int64_t> money(std::string_view)noexcept;static bool date(std::string_view)noexcept;
std::vector<FormFieldSpec> fields_;std::vector<FormValue> values_;app::RequestContext context_;std::vector<FormFieldError> errors_;std::optional<PresentedDomainError> form_error_;std::optional<std::string> saved_id_;std::uint64_t generation_{};bool pending_{};bool dirty_{};
};}

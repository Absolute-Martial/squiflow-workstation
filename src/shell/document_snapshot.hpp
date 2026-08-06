#pragma once
#include "app/contracts/result.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace squiflow::shell{enum class DocumentKind:std::uint8_t{Quotation,Invoice,Statement};struct DocumentLineSnapshot{std::string description,quantity,unit_price,amount;};struct PreparedDocumentSnapshot{DocumentKind kind;std::string id,number,title,party,date,period;std::vector<DocumentLineSnapshot> lines;std::string subtotal,tax,total,notes;};enum class PresentationErrorCode:std::uint8_t{InvalidSnapshot,MissingTemplate,RenderFailed,WriteFailed};struct PresentationError{PresentationErrorCode code;std::string message_key,detail;};app::Result<void,PresentationError> validate_document_snapshot(const PreparedDocumentSnapshot&);std::string document_template_name(DocumentKind);}

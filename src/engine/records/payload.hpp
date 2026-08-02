#pragma once

// The shape of a call payload.
//
// An operation arrives with a blob of fields: the customer's name, the rate,
// the quantity. Something has to say what that blob looks like, and it has to
// be one thing. If each module invented its own encoding, the sync
// orchestrator could not log a pending change, the outbox could not be
// inspected, and a payload written by an older version could not be read by a
// newer one.
//
// This is deliberately not MessagePack. MessagePack is the *wire* format
// between this machine and the shop server, and it is a dependency that has to
// be fetched, pinned and fuzzed. What is stored locally in the outbox needs
// something smaller: no schema, no external library, and a decoder that treats
// every length in the input as hostile, because a corrupted database file is
// the ordinary case on a spinning disk, not the exotic one.
//
// Format, all integers little-endian:
//
//   "SQF1"                       4 bytes
//   field count                  uint32
//   per field:
//     name length                uint16   (never zero)
//     name                       bytes
//     kind                       uint8    (ValueKind)
//     Null                       nothing
//     Integer                    int64
//     Real                       8 bytes, the IEEE-754 bit pattern
//     Text / Binary              uint32 length, then bytes
//
// Field order is preserved, so encoding a row and decoding it returns the same
// row, and encoding the same row twice returns identical bytes. That last part
// is what lets two devices compare a payload without agreeing on a hash of a
// map iteration order.

#include <stdexcept>
#include <string>

#include "engine/storage/store.hpp"

namespace squiflow::engine {

class PayloadError : public std::runtime_error {
public:
    explicit PayloadError(const std::string& message);
};

// Never fails: any Row that exists can be encoded.
Blob encode_payload(const Row& row);

// Throws PayloadError on anything it does not fully understand, including
// trailing bytes. A decoder that ignores what it does not recognise is a
// decoder that will one day accept half a record.
Row decode_payload(const Blob& bytes);

// Convenience for the common case of a payload holding one text field.
Blob encode_field(const std::string& column, const std::string& value);

}  // namespace squiflow::engine

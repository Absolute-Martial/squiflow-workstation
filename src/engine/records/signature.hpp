#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "engine/records/identity.hpp"

namespace squiflow::engine {

// A captured signature. Strokes are kept as well as the rendered image, so a
// disputed signature can be re-rendered at any size rather than blown up from
// a small bitmap.
struct Signature {
    RecordId id;
    std::string image_key;   // object store key for the rendered image
    std::uint32_t stroke_count = 0;
    std::uint32_t width_px = 0;
    std::uint32_t height_px = 0;
    Timestamp captured_at;
    PersonId witnessed_by;
};

// Signatures are never stored in a lossy format.
//
// This is not fussiness. Lossy compression destroys exactly what a signature
// is made of: thin, high-contrast strokes on a plain background. Bill photos
// are converted to a lossy format on purpose; evidence is not.
bool signature_format_allowed(std::string_view extension) noexcept;

}  // namespace squiflow::engine

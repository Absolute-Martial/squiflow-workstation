#include "shell/window_state.hpp"

#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace squiflow::shell {
namespace {

std::vector<std::string> split(const std::string& line, char separator) {
    std::vector<std::string> fields;
    std::string current;
    for (const char character : line) {
        if (character == separator) {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(character);
        }
    }
    fields.push_back(current);
    return fields;
}

std::optional<int> parse_int(const std::string& field) {
    int value = 0;
    const auto* begin = field.data();
    const auto* end = field.data() + field.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

std::optional<bool> parse_bool(const std::string& field) {
    if (field == "1") {
        return true;
    }
    if (field == "0") {
        return false;
    }
    return std::nullopt;
}

}  // namespace

bool is_valid_window_geometry(const WindowGeometry& geometry, int screen_width,
                              int screen_height) noexcept {
    if (screen_width <= 0 || screen_height <= 0) {
        // No known screen to validate against: accept only the maximized
        // case, since a maximized window has no meaningful stored bounds.
        return geometry.maximized;
    }
    if (geometry.maximized) {
        return true;
    }
    if (geometry.width < kMinimumWindowWidth ||
        geometry.height < kMinimumWindowHeight) {
        return false;
    }
    if (geometry.width > screen_width || geometry.height > screen_height) {
        return false;
    }
    // At least a corner of the window must land on screen, so it can always
    // be dragged back into view rather than being permanently unreachable.
    const bool horizontally_reachable =
        geometry.x < screen_width && geometry.x + geometry.width > 0;
    const bool vertically_reachable =
        geometry.y < screen_height && geometry.y + geometry.height > 0;
    return horizontally_reachable && vertically_reachable;
}

FileWindowStateStore::FileWindowStateStore(std::string file_path)
    : file_path_(std::move(file_path)) {}

std::optional<WindowGeometry> FileWindowStateStore::load() {
    std::ifstream stream(file_path_, std::ios::in | std::ios::binary);
    if (!stream.is_open()) {
        return std::nullopt;
    }
    std::string line;
    if (!std::getline(stream, line) || line.empty()) {
        return std::nullopt;
    }
    const auto fields = split(line, ',');
    if (fields.size() != 5) {
        return std::nullopt;
    }
    const auto x = parse_int(fields[0]);
    const auto y = parse_int(fields[1]);
    const auto width = parse_int(fields[2]);
    const auto height = parse_int(fields[3]);
    const auto maximized = parse_bool(fields[4]);
    if (!x || !y || !width || !height || !maximized) {
        return std::nullopt;
    }
    WindowGeometry geometry;
    geometry.x = *x;
    geometry.y = *y;
    geometry.width = *width;
    geometry.height = *height;
    geometry.maximized = *maximized;
    return geometry;
}

void FileWindowStateStore::save(const WindowGeometry& geometry) {
    std::ofstream stream(file_path_,
                         std::ios::out | std::ios::trunc | std::ios::binary);
    if (!stream.is_open()) {
        return;
    }
    stream << geometry.x << ',' << geometry.y << ',' << geometry.width << ','
           << geometry.height << ',' << (geometry.maximized ? '1' : '0')
           << '\n';
}

WindowGeometry resolve_window_geometry(WindowStateStore& store,
                                       int screen_width, int screen_height) {
    const auto restored = store.load();
    if (restored && is_valid_window_geometry(*restored, screen_width,
                                             screen_height)) {
        return *restored;
    }
    return kDefaultWindowGeometry;
}

}  // namespace squiflow::shell

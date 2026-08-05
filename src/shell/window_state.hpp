#pragma once

// Root window geometry: validated, persisted through the per-account cache
// location (never a Qt-native settings file, so it obeys the machine-wide vs
// per-account split already decided in ADR 0004), and always safe to load.
//
// A malformed, off-screen, or zero-sized restored geometry is never used as
// is -- it falls back to the default. Nothing here ever throws; a store
// failure is simply treated as "nothing to restore".

#include <optional>
#include <string>

namespace squiflow::shell {

struct WindowGeometry {
    int x = 0;
    int y = 0;
    int width = 1180;
    int height = 760;
    bool maximized = false;

    bool operator==(const WindowGeometry& other) const noexcept {
        return x == other.x && y == other.y && width == other.width &&
               height == other.height && maximized == other.maximized;
    }
};

inline constexpr WindowGeometry kDefaultWindowGeometry{};
inline constexpr int kMinimumWindowWidth = 640;
inline constexpr int kMinimumWindowHeight = 480;

// True when the geometry could plausibly be shown on a screen of the given
// size: width/height at least the minimum and no larger than the screen, and
// the window's origin leaves at least a corner of it on screen. A maximized
// geometry is always valid regardless of stored width/height/position,
// because those fields are ignored when maximized on restore.
bool is_valid_window_geometry(const WindowGeometry& geometry,
                              int screen_width, int screen_height) noexcept;

// Persists and restores exactly one WindowGeometry. Implementations must
// never throw; a failure to read or write is reported through the return
// value, not an exception.
class WindowStateStore {
public:
    virtual ~WindowStateStore() = default;

    // std::nullopt for "nothing stored" and for "stored, but unreadable" --
    // callers cannot tell those apart and do not need to: both fall back to
    // the default geometry.
    virtual std::optional<WindowGeometry> load() = 0;

    // Best-effort. A failure to persist geometry is not fatal to the running
    // application; the next restart simply falls back to the default again.
    virtual void save(const WindowGeometry& geometry) = 0;
};

// A store backed by one small text file, addressed by an already-resolved
// absolute path (typically inside PathRole::Cache). Confined to that one
// file: it never creates directories and never follows a path supplied by
// stored content.
class FileWindowStateStore final : public WindowStateStore {
public:
    explicit FileWindowStateStore(std::string file_path);

    std::optional<WindowGeometry> load() override;
    void save(const WindowGeometry& geometry) override;

private:
    std::string file_path_;
};

// Loads geometry from the store, validates it against the given screen
// bounds, and falls back to the default when missing or invalid. This is the
// one function callers use; nobody should call store.load() directly and
// skip validation.
WindowGeometry resolve_window_geometry(WindowStateStore& store,
                                       int screen_width, int screen_height);

}  // namespace squiflow::shell

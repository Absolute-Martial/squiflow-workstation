#include "shell/window_state.hpp"
#include "support/check.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>

namespace {

namespace t = squiflow::testing;
using squiflow::shell::FileWindowStateStore;
using squiflow::shell::is_valid_window_geometry;
using squiflow::shell::kDefaultWindowGeometry;
using squiflow::shell::resolve_window_geometry;
using squiflow::shell::WindowGeometry;
using squiflow::shell::WindowStateStore;

// A store that never touches disk, so ordering and fallback logic can be
// tested without depending on the file system.
class FakeWindowStateStore final : public WindowStateStore {
public:
    std::optional<WindowGeometry> load() override { return stored_; }
    void save(const WindowGeometry& geometry) override {
        ++save_count_;
        stored_ = geometry;
    }

    std::optional<WindowGeometry> stored_{};
    int save_count_ = 0;
};

std::string temp_file_path(const std::string& suffix) {
    const auto directory = std::filesystem::temp_directory_path();
    return (directory / ("squiflow_window_state_test_" + suffix + ".txt")).string();
}

}  // namespace

int main() {
    t::section("geometry validation against a known screen");
    {
        WindowGeometry reasonable{100, 100, 1180, 760, false};
        t::check(is_valid_window_geometry(reasonable, 1920, 1080),
                 "a normal window fits a normal screen");

        WindowGeometry too_small{100, 100, 200, 100, false};
        t::check(!is_valid_window_geometry(too_small, 1920, 1080),
                 "below the minimum size is rejected");

        WindowGeometry too_large{0, 0, 4000, 3000, false};
        t::check(!is_valid_window_geometry(too_large, 1920, 1080),
                 "larger than the screen is rejected");

        WindowGeometry off_screen{5000, 5000, 800, 600, false};
        t::check(!is_valid_window_geometry(off_screen, 1920, 1080),
                 "entirely off-screen is rejected");

        WindowGeometry negative_but_reachable{-100, -50, 800, 600, false};
        t::check(is_valid_window_geometry(negative_but_reachable, 1920, 1080),
                 "a corner still on screen is reachable");

        WindowGeometry maximized{-1, -1, 1, 1, true};
        t::check(is_valid_window_geometry(maximized, 1920, 1080),
                 "maximized ignores stored bounds");

        t::check(is_valid_window_geometry(maximized, 0, 0),
                 "maximized is accepted even with an unknown screen");
        WindowGeometry normal{100, 100, 800, 600, false};
        t::check(!is_valid_window_geometry(normal, 0, 0),
                 "a non-maximized geometry needs a known screen");
    }

    t::section("resolve falls back to the default when appropriate");
    {
        FakeWindowStateStore empty_store;
        const auto resolved = resolve_window_geometry(empty_store, 1920, 1080);
        t::check(resolved == kDefaultWindowGeometry,
                 "nothing stored resolves to the default");

        FakeWindowStateStore invalid_store;
        invalid_store.stored_ = WindowGeometry{9000, 9000, 100, 100, false};
        const auto resolved_invalid =
            resolve_window_geometry(invalid_store, 1920, 1080);
        t::check(resolved_invalid == kDefaultWindowGeometry,
                 "an invalid stored geometry falls back to the default");

        FakeWindowStateStore valid_store;
        valid_store.stored_ = WindowGeometry{50, 50, 1000, 700, false};
        const auto resolved_valid =
            resolve_window_geometry(valid_store, 1920, 1080);
        t::check(resolved_valid == *valid_store.stored_,
                 "a valid stored geometry is restored as is");
    }

    t::section("a restart round-trips through the fake store");
    {
        FakeWindowStateStore store;
        const WindowGeometry saved{20, 30, 1024, 768, false};
        store.save(saved);
        t::check(store.save_count_ == 1, "save is recorded exactly once");
        const auto restored = resolve_window_geometry(store, 1920, 1080);
        t::check(restored == saved, "geometry persists across a simulated restart");
    }

    t::section("file-backed store on a real disk");
    {
        const auto path = temp_file_path("roundtrip");
        std::remove(path.c_str());
        FileWindowStateStore store(path);
        t::check(!store.load().has_value(),
                 "a missing file has nothing to restore");

        const WindowGeometry saved{15, 25, 1100, 700, false};
        store.save(saved);
        const auto loaded = store.load();
        t::check(loaded.has_value() && *loaded == saved,
                 "saved geometry round-trips through the same file");

        const WindowGeometry maximized{0, 0, 1, 1, true};
        store.save(maximized);
        const auto loaded_maximized = store.load();
        t::check(loaded_maximized.has_value() && loaded_maximized->maximized,
                 "maximized state round-trips");
        std::remove(path.c_str());
    }

    t::section("malformed on-disk content never crashes and never restores");
    {
        const auto path = temp_file_path("malformed");

        std::ofstream empty_file(path, std::ios::trunc);
        empty_file.close();
        FileWindowStateStore empty_store(path);
        t::check(!empty_store.load().has_value(), "an empty file is refused");

        std::ofstream too_few_fields(path, std::ios::trunc);
        too_few_fields << "1,2,3\n";
        too_few_fields.close();
        FileWindowStateStore short_store(path);
        t::check(!short_store.load().has_value(),
                 "too few fields is refused, not partially parsed");

        std::ofstream non_numeric(path, std::ios::trunc);
        non_numeric << "not,a,number,here,0\n";
        non_numeric.close();
        FileWindowStateStore garbage_store(path);
        t::check(!garbage_store.load().has_value(),
                 "non-numeric fields are refused");

        std::ofstream bad_flag(path, std::ios::trunc);
        bad_flag << "1,2,3,4,maybe\n";
        bad_flag.close();
        FileWindowStateStore bad_flag_store(path);
        t::check(!bad_flag_store.load().has_value(),
                 "an unrecognised maximized flag is refused");

        std::remove(path.c_str());
        FileWindowStateStore unwritable_store("/nonexistent-directory/state.txt");
        unwritable_store.save(WindowGeometry{});
        t::check(!unwritable_store.load().has_value(),
                 "a save to an unwritable location fails silently and leaves nothing to load");
    }

    return t::report();
}

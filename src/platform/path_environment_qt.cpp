#include "platform/path_environment.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QStandardPaths>

#include <string>

// Discovery for the shipping Windows build.
//
// Not compiled by the verification lane: that machine has no Qt, so this file
// is proven by the Windows build and by the CMake lane with Qt present, and
// the quality gate says so rather than pretending otherwise. Everything it
// decides is handed to the same PathResolver that the strict suite exercises
// exhaustively, so the untested surface here is discovery alone.
//
// Why the standard-locations list rather than a known-folder call: Qt already
// asks Windows for the same folders, the list is ordered per-account first and
// machine-wide second, and using it keeps the Windows header out of the
// codebase entirely. See docs/adr/0004-machine-wide-data-root.md.

namespace squiflow::platform {
namespace {

std::string to_utf8(const QString& text) {
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString cleaned(const QString& path) {
    return QDir::fromNativeSeparators(QDir::cleanPath(path));
}

// True when the candidate is the per-account location or lives inside the
// installation directory. Both are disqualified: the first hides the shop's
// records from other accounts, the second is normally read-only and is
// replaced wholesale on upgrade.
bool is_per_account_or_installed(const QString& candidate,
                                 const QString& writable,
                                 const QString& installed) {
    if (candidate.compare(writable, Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (!installed.isEmpty()
        && candidate.startsWith(installed, Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

QString machine_wide_data_root() {
    const QString writable =
        cleaned(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const QString installed =
        QCoreApplication::instance() == nullptr
            ? QString()
            : cleaned(QCoreApplication::applicationDirPath());

    const QStringList candidates =
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    for (const QString& raw : candidates) {
        const QString candidate = cleaned(raw);
        if (candidate.isEmpty()) {
            continue;
        }
        if (is_per_account_or_installed(candidate, writable, installed)) {
            continue;
        }
        return candidate;
    }
    return QString();
}

}  // namespace

EnvironmentDiscovery discover_path_environment(const ApplicationIdentity& identity) {
    EnvironmentDiscovery discovery;
    discovery.environment.identity = identity;

    const QString override_shared =
        cleaned(qEnvironmentVariable(kSharedDataRootVariable));
    const QString shared =
        override_shared.isEmpty() ? machine_wide_data_root() : override_shared;

    if (shared.isEmpty()) {
        discovery.ok = false;
        discovery.message =
            "this machine reports no shared application data location. The "
            "shop's records must not be placed in a single account's folder, "
            "so startup cannot continue; set the documented data root "
            "override or repair the Windows folder configuration.";
        return discovery;
    }

    QString cache = cleaned(qEnvironmentVariable(kUserCacheRootVariable));
    if (cache.isEmpty()) {
        cache = cleaned(
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    }

    discovery.environment.shared_data_root = to_utf8(shared);
    discovery.environment.user_cache_root = to_utf8(cache);
    discovery.ok = true;
    discovery.message = override_shared.isEmpty()
                            ? "shared data root taken from the machine-wide "
                              "application data location"
                            : "shared data root taken from the environment "
                              "override";
    return discovery;
}

}  // namespace squiflow::platform

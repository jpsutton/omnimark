#pragma once

#include <QIcon>

namespace Icons {

// Theme icon by freedesktop name. On Windows/macOS, Qt maps standard names
// to native icons; names without a native match fall back to an icon bundled
// in :/resources/icons/<fallback>.svg (if given).
QIcon get(const char *themeName, const char *fallback = nullptr);

// Linux without a desktop integration plugin (e.g. AppImage) reports no icon
// theme; pick an installed one so toolbars aren't text-only.
void ensureTheme();

}

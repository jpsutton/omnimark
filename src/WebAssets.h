#pragma once

#include <QString>

namespace WebAssets {

// Directory holding the built web frontend (index.html, assets/).
// Lookup order: $OMNIMARK_WEB_DIR, <appdir>/web, <appdir>/../share/omnimark/web,
// <appdir>/usr/share/omnimark/web (AppImage),
// <appdir>/../Resources/web (macOS bundle), then the source tree's web/dist.
QString directory();

// Dev server origin from $OMNIMARK_DEV_URL (e.g. http://localhost:5173), or empty.
QString devServerUrl();

}

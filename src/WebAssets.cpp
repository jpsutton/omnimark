#include "WebAssets.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace WebAssets {

static bool hasIndex(const QString &dir)
{
    return QFileInfo::exists(dir + QStringLiteral("/index.html"));
}

QString directory()
{
    const QString env = qEnvironmentVariable("OMNIMARK_WEB_DIR");
    if (!env.isEmpty() && hasIndex(env))
        return QDir(env).canonicalPath();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/web"),
        appDir + QStringLiteral("/../share/omnimark/web"),
        appDir + QStringLiteral("/usr/share/omnimark/web"), // AppImage: AppRun at the AppDir root
        appDir + QStringLiteral("/../Resources/web"),
        QStringLiteral(OMNIMARK_SOURCE_WEB_DIST),
    };
    for (const QString &candidate : candidates) {
        if (hasIndex(candidate))
            return QDir(candidate).canonicalPath();
    }
    return {};
}

QString devServerUrl()
{
    QString url = qEnvironmentVariable("OMNIMARK_DEV_URL");
    while (url.endsWith(u'/'))
        url.chop(1);
    return url;
}

}

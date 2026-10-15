#include "Icons.h"

#include <QDir>
#include <QFileInfo>

namespace Icons {

QIcon get(const char *themeName, const char *fallback)
{
    const QString name = QString::fromLatin1(themeName);
    if (QIcon::hasThemeIcon(name))
        return QIcon::fromTheme(name);
    if (fallback) {
        const QString path = QStringLiteral(":/resources/icons/%1.svg").arg(QString::fromLatin1(fallback));
        if (QFileInfo::exists(path))
            return QIcon(path);
    }
    return QIcon::fromTheme(name);
}

void ensureTheme()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    const QString current = QIcon::themeName();
    if (!current.isEmpty() && current != u"hicolor")
        return;
    static const QStringList candidates = {
        QStringLiteral("breeze"), QStringLiteral("Adwaita"), QStringLiteral("Papirus"),
        QStringLiteral("elementary"), QStringLiteral("Yaru"),
    };
    for (const QString &theme : candidates) {
        for (const QString &base : QIcon::themeSearchPaths()) {
            if (QFileInfo::exists(base + u'/' + theme + QStringLiteral("/index.theme"))) {
                QIcon::setThemeName(theme);
                return;
            }
        }
    }
#endif
}

}

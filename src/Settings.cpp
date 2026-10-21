#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    s.beginGroup(QStringLiteral("settings"));
    m_theme = Theme(qBound(0, s.value(QStringLiteral("theme"), 0).toInt(), 2));
    m_previewStyle = s.value(QStringLiteral("previewStyle"), m_previewStyle).toString();
    m_previewFontSize = s.value(QStringLiteral("previewFontSize"), m_previewFontSize).toInt();
    m_previewWidth = s.value(QStringLiteral("previewWidth"), m_previewWidth).toInt();
    m_codeLineNumbers = s.value(QStringLiteral("codeLineNumbers"), m_codeLineNumbers).toBool();
    m_remoteImages = s.value(QStringLiteral("remoteImages"), m_remoteImages).toBool();
    m_editorFontSize = s.value(QStringLiteral("editorFontSize"), m_editorFontSize).toInt();
    m_vim = s.value(QStringLiteral("vim"), m_vim).toBool();
    m_wordWrap = s.value(QStringLiteral("wordWrap"), m_wordWrap).toBool();
    m_syncScroll = s.value(QStringLiteral("syncScroll"), m_syncScroll).toBool();
    m_defaultViewMode = s.value(QStringLiteral("defaultViewMode"), m_defaultViewMode).toString();
    m_plantumlServer = s.value(QStringLiteral("plantumlServer")).toString();
    m_toolbarStyle = s.value(QStringLiteral("toolbarStyle"), m_toolbarStyle).toString();
    m_checkForUpdates = s.value(QStringLiteral("checkForUpdates"), m_checkForUpdates).toBool();
    s.endGroup();

    connect(&m_cssWatcher, &QFileSystemWatcher::fileChanged, this, [this] {
        loadCustomCss();
        emit changed();
    });
    connect(&m_cssWatcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        const QString previous = m_customCss;
        loadCustomCss();
        if (previous != m_customCss)
            emit changed();
    });
    loadCustomCss();
}

void Settings::save()
{
    QSettings s;
    s.beginGroup(QStringLiteral("settings"));
    s.setValue(QStringLiteral("theme"), int(m_theme));
    s.setValue(QStringLiteral("previewStyle"), m_previewStyle);
    s.setValue(QStringLiteral("previewFontSize"), m_previewFontSize);
    s.setValue(QStringLiteral("previewWidth"), m_previewWidth);
    s.setValue(QStringLiteral("codeLineNumbers"), m_codeLineNumbers);
    s.setValue(QStringLiteral("remoteImages"), m_remoteImages);
    s.setValue(QStringLiteral("editorFontSize"), m_editorFontSize);
    s.setValue(QStringLiteral("vim"), m_vim);
    s.setValue(QStringLiteral("wordWrap"), m_wordWrap);
    s.setValue(QStringLiteral("syncScroll"), m_syncScroll);
    s.setValue(QStringLiteral("defaultViewMode"), m_defaultViewMode);
    s.setValue(QStringLiteral("plantumlServer"), m_plantumlServer);
    s.setValue(QStringLiteral("toolbarStyle"), m_toolbarStyle);
    s.setValue(QStringLiteral("checkForUpdates"), m_checkForUpdates);
    s.endGroup();
    emit changed();
}

#define OMNIMARK_SETTER(name, member, type)                                                        \
    void Settings::name(type value)                                                                \
    {                                                                                              \
        if (member == value)                                                                       \
            return;                                                                                \
        member = value;                                                                            \
        save();                                                                                    \
    }

OMNIMARK_SETTER(setTheme, m_theme, Theme)
OMNIMARK_SETTER(setPreviewStyle, m_previewStyle, const QString &)
OMNIMARK_SETTER(setPreviewFontSize, m_previewFontSize, int)
OMNIMARK_SETTER(setPreviewWidth, m_previewWidth, int)
OMNIMARK_SETTER(setCodeLineNumbers, m_codeLineNumbers, bool)
OMNIMARK_SETTER(setRemoteImages, m_remoteImages, bool)
OMNIMARK_SETTER(setEditorFontSize, m_editorFontSize, int)
OMNIMARK_SETTER(setVim, m_vim, bool)
OMNIMARK_SETTER(setWordWrap, m_wordWrap, bool)
OMNIMARK_SETTER(setSyncScroll, m_syncScroll, bool)
OMNIMARK_SETTER(setDefaultViewMode, m_defaultViewMode, const QString &)
OMNIMARK_SETTER(setPlantumlServer, m_plantumlServer, const QString &)
OMNIMARK_SETTER(setToolbarStyle, m_toolbarStyle, const QString &)
OMNIMARK_SETTER(setCheckForUpdates, m_checkForUpdates, bool)

#undef OMNIMARK_SETTER

QString Settings::customCssPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/custom.css");
}

void Settings::ensureCustomCss()
{
    const QString path = customCssPath();
    if (QFileInfo::exists(path))
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write("/* Omnimark custom CSS. Saved changes apply immediately to every open document.\n"
                   " *\n"
                   " * Useful hooks:\n"
                   " *   .markdown-body                  the rendered document\n"
                   " *   :root[data-theme=\"dark\"]        dark theme overrides\n"
                   " *   --om-font, --om-mono, --om-link   theme variables (see the defaults in the app)\n"
                   " */\n\n");
    }
    loadCustomCss();
}

void Settings::loadCustomCss()
{
    const QString path = customCssPath();
    const QString dir = QFileInfo(path).absolutePath();
    QFile file(path);
    m_customCss = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    // Watch the directory too: editors that save atomically replace the file.
    if (QFileInfo::exists(dir) && !m_cssWatcher.directories().contains(dir))
        m_cssWatcher.addPath(dir);
    if (QFileInfo::exists(path) && !m_cssWatcher.files().contains(path))
        m_cssWatcher.addPath(path);
}

QString Settings::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("previewStyle")] = m_previewStyle;
    o[QStringLiteral("previewFontSize")] = m_previewFontSize;
    o[QStringLiteral("previewWidth")] = m_previewWidth;
    o[QStringLiteral("lineNumbers")] = m_codeLineNumbers;
    o[QStringLiteral("remoteImages")] = m_remoteImages;
    o[QStringLiteral("editorFontSize")] = m_editorFontSize;
    o[QStringLiteral("vim")] = m_vim;
    o[QStringLiteral("wrap")] = m_wordWrap;
    o[QStringLiteral("syncScroll")] = m_syncScroll;
    o[QStringLiteral("customCss")] = m_customCss;
    o[QStringLiteral("plantumlServer")] = m_plantumlServer;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

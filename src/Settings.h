#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

// App preferences backed by QSettings. Changes are broadcast through
// changed() so every open document picks them up immediately.
class Settings : public QObject
{
    Q_OBJECT
public:
    enum class Theme { System, Light, Dark };
    Q_ENUM(Theme)

    explicit Settings(QObject *parent = nullptr);

    Theme theme() const { return m_theme; }
    QString previewStyle() const { return m_previewStyle; } // github | serif | compact
    int previewFontSize() const { return m_previewFontSize; }
    int previewWidth() const { return m_previewWidth; } // 0 = unlimited
    bool codeLineNumbers() const { return m_codeLineNumbers; }
    bool remoteImages() const { return m_remoteImages; }
    int editorFontSize() const { return m_editorFontSize; }
    bool vim() const { return m_vim; }
    bool wordWrap() const { return m_wordWrap; }
    bool syncScroll() const { return m_syncScroll; }
    QString defaultViewMode() const { return m_defaultViewMode; } // preview | split | source
    QString plantumlServer() const { return m_plantumlServer; } // empty = PlantUML disabled
    QString toolbarStyle() const { return m_toolbarStyle; } // icons | beside | under

    void setTheme(Theme theme);
    void setPreviewStyle(const QString &style);
    void setPreviewFontSize(int size);
    void setPreviewWidth(int width);
    void setCodeLineNumbers(bool on);
    void setRemoteImages(bool on);
    void setEditorFontSize(int size);
    void setVim(bool on);
    void setWordWrap(bool on);
    void setSyncScroll(bool on);
    void setDefaultViewMode(const QString &mode);
    void setPlantumlServer(const QString &url);
    void setToolbarStyle(const QString &style);

    // Contents of <config>/custom.css (empty if none); hot-reloaded.
    QString customCssPath() const;
    QString customCss() const { return m_customCss; }
    // Creates custom.css with a commented template if missing.
    void ensureCustomCss();

    // Settings for the web page (see Settings in web/src/main.ts).
    QString toJson() const;

signals:
    void changed();

private:
    void save();
    void loadCustomCss();

    Theme m_theme = Theme::System;
    QString m_previewStyle = QStringLiteral("github");
    int m_previewFontSize = 16;
    int m_previewWidth = 980;
    bool m_codeLineNumbers = true;
    bool m_remoteImages = true;
    int m_editorFontSize = 14;
    bool m_vim = false;
    bool m_wordWrap = true;
    bool m_syncScroll = true;
    QString m_defaultViewMode = QStringLiteral("preview");
    QString m_plantumlServer;
    QString m_toolbarStyle = QStringLiteral("icons");
    QString m_customCss;
    QFileSystemWatcher m_cssWatcher;
};

#pragma once

#include <QStringList>
#include <QVariantList>
#include <QWidget>

#include <functional>

class Bridge;
class FileWatcher;
class LocalRoots;
class QWebChannel;
class QWebEngineProfile;
class QWebEngineView;
class WebPage;

// One open document: a web view running the frontend, its QWebChannel
// bridge, and a watcher that pushes on-disk changes into the page. Following
// a link to another Markdown file navigates within the tab, with history.
class DocumentTab : public QWidget
{
    Q_OBJECT
public:
    DocumentTab(QWebEngineProfile *profile, LocalRoots *roots, QWidget *parent = nullptr);
    ~DocumentTab() override;

    enum class ViewMode { Preview, Split, Source };
    Q_ENUM(ViewMode)

    // Replaces the document without touching history.
    bool load(const QString &path, QString *error = nullptr);

    // An empty, unsaved document (needs Save As).
    void loadUntitled(const QString &text = {});
    bool isUntitled() const { return m_path.isEmpty(); }

    // Unsaved edits in the editor.
    bool isDirty() const { return m_dirty; }

    // Writes the editor contents to path() (or `path` for Save As). `done`
    // receives success and an error message.
    using SaveCallback = std::function<void(bool ok, const QString &error)>;
    void save(const SaveCallback &done);
    void saveAs(const QString &path, const SaveCallback &done);

    // Throws away unsaved edits and shows the file as it is on disk.
    void revert();
    // Resolves a conflict in favor of the editor; later disk changes ask again.
    void keepLocalVersion();

    ViewMode viewMode() const { return m_viewMode; }
    void setViewMode(ViewMode mode);
    void setTheme(const QString &theme);
    QString theme() const { return m_theme; }
    void setSettingsJson(const QString &json);
    // Scrolls once the current document has rendered (immediately if it has).
    void scrollToLine(double line);

    // Runs JS in the page and hands back the result (no-op before the page exists).
    void evaluate(const QString &script, const std::function<void(const QVariant &)> &callback);

    // Opens `path` in this tab, pushing the current document onto history.
    bool navigate(const QString &path, const QString &fragment = {}, QString *error = nullptr);
    bool canGoBack() const { return !m_back.isEmpty(); }
    bool canGoForward() const { return !m_forward.isEmpty(); }
    void goBack();
    void goForward();

    void scrollToAnchor(const QString &id);

    QString path() const { return m_path; }
    QString title() const;
    QVariantList headings() const { return m_headings; }
    int wordCount() const { return m_words; }

    // The page is created the first time the tab is shown, so restoring a
    // session with many tabs doesn't start many renderers at once.
    void ensurePage();

    QWebEngineView *view() const { return m_view; }
    WebPage *page() const { return m_page; }
    Bridge *bridge() const { return m_bridge; }

    static QUrl appUrl();

signals:
    void rendered(int elapsedMs);
    void openUrlRequested(const QUrl &url, bool newTab);
    void fileRemoved();
    void pathChanged(const QString &path);
    void historyChanged();
    void headingsChanged(const QVariantList &headings);
    void activeHeadingChanged(const QString &id);
    void dirtyChanged(bool dirty);
    void viewModeChanged(DocumentTab::ViewMode mode);
    // The file changed on disk while there are unsaved edits.
    void externalChangeConflict();
    void localFileDropped(const QString &path);
    void statsChanged(int words, int characters);

protected:
    void showEvent(QShowEvent *event) override;
    // Takes file drops before Chromium sees them (it would navigate to them).
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool readFile(const QString &path, QString *text, QString *error) const;
    void setDocument(const QString &path, const QString &text);
    void writeFile(const QString &path, const SaveCallback &done);
    void setDirty(bool dirty);
    void pushState();
    void showContextMenu(const QPoint &pos);
    void insertImageLinks(const QStringList &paths);
    static bool isImageFile(const QString &path);
    void pushDocument();
    void reload();

    QWebEngineProfile *m_profile;
    LocalRoots *m_roots;
    QWebEngineView *m_view = nullptr;
    WebPage *m_page = nullptr;
    QWebChannel *m_channel = nullptr;
    Bridge *m_bridge;
    FileWatcher *m_watcher;
    QString m_path;
    QString m_text;
    QString m_rootKey;
    QString m_pendingAnchor;
    double m_pendingLine = 0;
    QString m_renderedPath;
    QStringList m_back;
    QStringList m_forward;
    QVariantList m_headings;
    QString m_theme;
    QString m_settingsJson;
    ViewMode m_viewMode = ViewMode::Preview;
    bool m_pageReady = false;
    bool m_dirty = false;
    bool m_conflictPending = false;
    int m_words = 0;
};

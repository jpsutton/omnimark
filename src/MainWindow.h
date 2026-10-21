#pragma once

#include <QMainWindow>

#include <functional>

class DocumentTab;
class Exporter;
class Settings;
class FindBar;
class SearchDock;
class TocDock;
class Updater;
class LocalRoots;
class QAction;
class QActionGroup;
class QLabel;
class QToolBar;
class QMenu;
class QTabWidget;
class QWebEngineProfile;
class WorkspaceDock;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWebEngineProfile *profile, LocalRoots *roots, Settings *settings, QWidget *parent = nullptr);

    // Opens `path` in a new tab, or focuses the tab already showing it.
    DocumentTab *openFile(const QString &path);
    // Opens `path` in the current tab (with history), or a new tab if none.
    DocumentTab *openInCurrentTab(const QString &path, const QString &fragment = {});
    DocumentTab *currentTab() const;
    QList<DocumentTab *> tabs() const;

    DocumentTab *newDocument();
    void openFolder(const QString &path);
    void restoreSession();

    static bool isMarkdownFile(const QString &path);
    // Resolves a link target that may be missing its extension or live
    // elsewhere in the workspace ([[wiki links]]). Returns empty if not found.
    static QString resolveDocument(const QString &path, const QStringList &searchRoots);
    // False for programs, scripts and installers: links never launch those.
    static bool isSafeToOpen(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void createActions();
    DocumentTab *createTab(const QString &path, bool activate);
    void showOpenDialog();
    void showOpenFolderDialog();
    void closeTab(int index);
    // Asks about unsaved edits; calls `proceed` once it's OK to discard the tab.
    void confirmClose(DocumentTab *tab, const std::function<void()> &proceed);
    void saveTab(DocumentTab *tab, const std::function<void(bool)> &done = {});
    void saveTabAs(DocumentTab *tab, const std::function<void(bool)> &done = {});
    void updateTabLabel(DocumentTab *tab);
    void updateViewModeActions();
    void setCurrentViewMode(int mode);
    QString currentThemeName() const;
    void applyThemeToTabs();
    void startFind();
    void runFind(const QString &text, bool backward, bool caseSensitive, bool refresh = false);
    void syncDocks();
    void updateStats();
    void applySettingsToTabs();
    void applyToolbarStyle();
    void exportPdf();
    void exportHtml();
    void showShortcuts();
    void showAbout();
    void setUpUpdates();
    // `interactive`: the user asked, so report "up to date" and errors too.
    void checkForUpdates(bool interactive);
    void offerRestartForUpdate();
    QString exportPath(const QString &title, const QString &suffix, const QString &filter);
    void handleUrl(DocumentTab *source, const QUrl &url, bool newTab);
    void updateTitle();
    void updateNavigation();
    void addRecentFile(const QString &path);
    void rebuildRecentMenu();
    void saveSession() const;
    int indexOf(DocumentTab *tab) const;

    QWebEngineProfile *m_profile;
    LocalRoots *m_roots;
    Settings *m_settings;
    Exporter *m_exporter;
    QTabWidget *m_tabs;
    WorkspaceDock *m_workspace;
    TocDock *m_toc;
    SearchDock *m_search;
    FindBar *m_findBar;
    QMenu *m_recentMenu = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_revertAction = nullptr;
    QActionGroup *m_viewModeGroup = nullptr;
    QAction *m_editorAction = nullptr; // Preview <-> Split toggle (pressed while the editor shows)
    QToolBar *m_toolbar = nullptr;
    QActionGroup *m_toolbarStyleGroup = nullptr;
    QLabel *m_statsLabel = nullptr;
    Updater *m_updater = nullptr;
    QAction *m_updateAction = nullptr;
    QString m_updateVersion;
    bool m_interactiveUpdateCheck = false;
    bool m_relaunchAfterUpdate = false;
    QString m_workspaceRootKey;
    bool m_closing = false;
};

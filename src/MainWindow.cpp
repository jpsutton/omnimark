#include "MainWindow.h"

#include "DocumentTab.h"
#include "Icons.h"
#include "Exporter.h"
#include "FindBar.h"
#include "LocalRoots.h"
#include "SearchDock.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "TocDock.h"
#include "WorkspaceDock.h"

#include <QAction>
#include <QLabel>
#include <QActionGroup>
#include <QPointer>
#include <QPushButton>
#include <QStyleHints>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QLocale>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>

#include <memory>

using namespace Qt::StringLiterals;

static constexpr int MaxRecentFiles = 12;

MainWindow::MainWindow(QWebEngineProfile *profile, LocalRoots *roots, Settings *settings, QWidget *parent)
    : QMainWindow(parent)
    , m_profile(profile)
    , m_roots(roots)
    , m_settings(settings)
    , m_exporter(new Exporter(this))
    , m_tabs(new QTabWidget(this))
    , m_workspace(new WorkspaceDock(this))
    , m_toc(new TocDock(this))
    , m_search(new SearchDock(this))
    , m_findBar(new FindBar)
{
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->tabBar()->setElideMode(Qt::ElideMiddle);
    auto *central = new QWidget;
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_tabs, 1);
    centralLayout->addWidget(m_findBar);
    m_findBar->hide();
    setCentralWidget(central);
    setAcceptDrops(true);
    resize(1200, 820);

    addDockWidget(Qt::LeftDockWidgetArea, m_workspace);
    addDockWidget(Qt::LeftDockWidgetArea, m_search);
    tabifyDockWidget(m_workspace, m_search);
    addDockWidget(Qt::RightDockWidgetArea, m_toc);
    m_workspace->hide();
    m_search->hide();
    m_toc->hide();
    m_search->setRoot(QString());

    connect(m_workspace, &WorkspaceDock::rootChanged, m_search, &SearchDock::setRoot);
    connect(m_toc, &TocDock::headingActivated, this, [this](const QString &id, int line) {
        DocumentTab *tab = currentTab();
        if (!tab)
            return;
        if (tab->viewMode() == DocumentTab::ViewMode::Source && line > 0)
            tab->scrollToLine(line);
        else
            tab->scrollToAnchor(id);
    });
    connect(m_search, &SearchDock::hitActivated, this, [this](const QString &path, int line) {
        DocumentTab *tab = openFile(path);
        if (tab)
            tab->scrollToLine(line);
    });
    connect(m_findBar, &FindBar::findRequested, this,
            [this](const QString &text, bool backward, bool caseSensitive) { runFind(text, backward, caseSensitive); });
    connect(m_findBar, &FindBar::closed, this, [this] {
        if (DocumentTab *tab = currentTab())
            tab->evaluate(QStringLiteral("window.omnimark.clearFind()"), [](const QVariant &) {});
    });

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        updateTitle();
        updateNavigation();
        updateViewModeActions();
        syncDocks();
        updateStats();
        if (DocumentTab *tab = currentTab())
            m_workspace->reveal(tab->path());
    });
    connect(m_tabs->tabBar(), &QTabBar::tabBarClicked, this, [this](int index) {
        if (index >= 0 && QApplication::mouseButtons() & Qt::MiddleButton)
            closeTab(index);
    });
    connect(m_workspace, &WorkspaceDock::fileActivated, this, [this](const QString &path, bool newTab) {
        if (newTab)
            openFile(path);
        else
            openInCurrentTab(path);
    });

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &MainWindow::applyThemeToTabs);
    connect(m_settings, &Settings::changed, this, [this] {
        applyThemeToTabs();
        applySettingsToTabs();
        applyToolbarStyle();
    });

    m_statsLabel = new QLabel;
    statusBar()->addPermanentWidget(m_statsLabel);

    createActions();
    rebuildRecentMenu();
    updateTitle();
    updateNavigation();
    updateViewModeActions();
    applyToolbarStyle();
}

void MainWindow::createActions()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));

    QAction *newDoc = file->addAction(Icons::get("document-new"), tr("&New"),
                                      this, &MainWindow::newDocument);
    newDoc->setShortcut(QKeySequence::New);

    QAction *open = file->addAction(Icons::get("document-open"), tr("&Open…"),
                                    this, &MainWindow::showOpenDialog);
    open->setShortcut(QKeySequence::Open);

    QAction *openFolder = file->addAction(Icons::get("folder-open"),
                                          tr("Open &Folder…"), this, &MainWindow::showOpenFolderDialog);
    openFolder->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

    m_recentMenu = file->addMenu(tr("Open &Recent"));

    file->addSeparator();
    m_saveAction = file->addAction(Icons::get("document-save"), tr("&Save"), this, [this] {
        if (DocumentTab *tab = currentTab())
            saveTab(tab);
    });
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAsAction = file->addAction(Icons::get("document-save-as"), tr("Save &As…"), this, [this] {
        if (DocumentTab *tab = currentTab())
            saveTabAs(tab);
    });
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_revertAction = file->addAction(Icons::get("document-revert"), tr("Re&vert"), this, [this] {
        DocumentTab *tab = currentTab();
        if (!tab || tab->isUntitled())
            return;
        if (QMessageBox::question(this, tr("Revert"), tr("Discard unsaved changes to %1?").arg(tab->title()))
            == QMessageBox::Yes)
            tab->revert();
    });

    file->addAction(Icons::get("document-open-folder"),
                    tr("Reveal in File &Manager"), this, [this] {
        DocumentTab *tab = currentTab();
        if (tab && !tab->isUntitled())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(tab->path()).absolutePath()));
    });

    file->addSeparator();
    QAction *exportPdfAction = file->addAction(Icons::get("application-pdf"),
                                               tr("Export as &PDF…"), this, &MainWindow::exportPdf);
    exportPdfAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    file->addAction(Icons::get("text-html"), tr("Export as &HTML…"), this,
                    &MainWindow::exportHtml);
    QAction *printAction = file->addAction(Icons::get("document-print"), tr("&Print…"), this, [this] {
        if (DocumentTab *tab = currentTab())
            m_exporter->print(tab, this);
    });
    printAction->setShortcut(QKeySequence::Print);

    file->addSeparator();
    QAction *prefs = file->addAction(Icons::get("preferences-system", "settings"), tr("Se&ttings…"), this, [this] {
        SettingsDialog dialog(m_settings, this);
        connect(&dialog, &SettingsDialog::editCustomCssRequested, this, [this] {
            m_settings->ensureCustomCss();
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_settings->customCssPath()));
        });
        dialog.exec();
    });
    prefs->setShortcut(QKeySequence::Preferences);
    if (prefs->shortcut().isEmpty())
        prefs->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    prefs->setMenuRole(QAction::PreferencesRole);

    file->addSeparator();
    QAction *close = file->addAction(tr("&Close Tab"), this, [this] {
        if (m_tabs->currentIndex() >= 0)
            closeTab(m_tabs->currentIndex());
    });
    close->setShortcut(QKeySequence::Close);

    file->addSeparator();
    QAction *quit = file->addAction(tr("&Quit"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);
    quit->setMenuRole(QAction::QuitRole);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    QAction *findAction = edit->addAction(Icons::get("edit-find"), tr("&Find…"),
                                          this, &MainWindow::startFind);
    findAction->setShortcut(QKeySequence::Find);
    QAction *findNext = edit->addAction(tr("Find &Next"), this, [this] {
        if (m_findBar->isVisible())
            runFind(m_findBar->text(), false, m_findBar->caseSensitive());
    });
    findNext->setShortcut(QKeySequence::FindNext);
    QAction *findPrev = edit->addAction(tr("Find &Previous"), this, [this] {
        if (m_findBar->isVisible())
            runFind(m_findBar->text(), true, m_findBar->caseSensitive());
    });
    findPrev->setShortcut(QKeySequence::FindPrevious);
    edit->addSeparator();
    QAction *findInFolder = edit->addAction(Icons::get("edit-find-replace"),
                                            tr("Find in &Folder…"), this, [this] {
        m_search->activate();
    });
    findInFolder->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));

    QMenu *go = menuBar()->addMenu(tr("&Go"));
    m_backAction = go->addAction(Icons::get("go-previous"), tr("&Back"), this, [this] {
        if (DocumentTab *tab = currentTab())
            tab->goBack();
    });
    m_backAction->setShortcuts(QKeySequence::Back);
    m_forwardAction = go->addAction(Icons::get("go-next"), tr("&Forward"), this, [this] {
        if (DocumentTab *tab = currentTab())
            tab->goForward();
    });
    m_forwardAction->setShortcuts(QKeySequence::Forward);
    go->addSeparator();
    QAction *nextTab = go->addAction(tr("&Next Tab"), this, [this] {
        if (m_tabs->count())
            m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count());
    });
    nextTab->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_PageDown), QKeySequence(Qt::CTRL | Qt::Key_Tab) });
    QAction *prevTab = go->addAction(tr("&Previous Tab"), this, [this] {
        if (m_tabs->count())
            m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count());
    });
    prevTab->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_PageUp), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab) });

    QMenu *view = menuBar()->addMenu(tr("&View"));
    m_viewModeGroup = new QActionGroup(this);
    const struct {
        const char *text;
        const char *icon;
        const char *fallback;
        Qt::Key key;
    } modes[] = {
        { QT_TR_NOOP("&Preview"), "view-preview", "view-preview", Qt::Key_1 },
        { QT_TR_NOOP("&Split"), "view-split-left-right", "view-split", Qt::Key_2 },
        { QT_TR_NOOP("S&ource"), "document-edit", "view-source", Qt::Key_3 },
    };
    for (int i = 0; i < 3; ++i) {
        QAction *action = view->addAction(Icons::get(modes[i].icon, modes[i].fallback), tr(modes[i].text));
        action->setCheckable(true);
        action->setData(i);
        action->setShortcut(QKeySequence(Qt::CTRL | modes[i].key));
        m_viewModeGroup->addAction(action);
    }
    connect(m_viewModeGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        setCurrentViewMode(action->data().toInt());
    });
    // One toolbar button for the common case: show/hide the editor next to
    // the preview. Its drop-down (and the menu above) offers all three modes.
    m_editorAction = view->addAction(Icons::get("view-split-left-right", "view-split"), tr("&Editor"));
    m_editorAction->setCheckable(true);
    m_editorAction->setObjectName(QStringLiteral("editorToggle"));
    m_editorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_editorAction, &QAction::triggered, this, [this](bool show) {
        setCurrentViewMode(show ? int(DocumentTab::ViewMode::Split) : int(DocumentTab::ViewMode::Preview));
    });

    QMenu *labels = view->addMenu(tr("Toolbar &Labels"));
    m_toolbarStyleGroup = new QActionGroup(this);
    const struct {
        const char *text;
        const char *value;
    } styles[] = {
        { QT_TR_NOOP("&Icons Only"), "icons" },
        { QT_TR_NOOP("Text &Beside Icons"), "beside" },
        { QT_TR_NOOP("Text &Under Icons"), "under" },
    };
    for (const auto &style : styles) {
        QAction *action = labels->addAction(tr(style.text));
        action->setCheckable(true);
        action->setData(QString::fromLatin1(style.value));
        m_toolbarStyleGroup->addAction(action);
    }
    connect(m_toolbarStyleGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        m_settings->setToolbarStyle(action->data().toString());
    });
    view->addSeparator();
    QAction *toggleFiles = m_workspace->toggleViewAction();
    toggleFiles->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    toggleFiles->setIcon(QIcon::hasThemeIcon(QStringLiteral("sidebar-show-symbolic"))
                             ? Icons::get("sidebar-show-symbolic")
                             : Icons::get("view-sidetree", "sidebar"));
    view->addAction(toggleFiles);
    QAction *toggleToc = m_toc->toggleViewAction();
    toggleToc->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    toggleToc->setIcon(Icons::get("format-list-unordered", "outline"));
    view->addAction(toggleToc);
    view->addAction(m_search->toggleViewAction());

    view->addSeparator();
    QAction *zoomIn = view->addAction(Icons::get("zoom-in"), tr("Zoom &In"), this, [this] {
        m_settings->setPreviewFontSize(qMin(32, m_settings->previewFontSize() + 1));
        m_settings->setEditorFontSize(qMin(32, m_settings->editorFontSize() + 1));
    });
    zoomIn->setShortcuts({ QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal) });
    QAction *zoomOut = view->addAction(Icons::get("zoom-out"), tr("Zoom &Out"), this, [this] {
        m_settings->setPreviewFontSize(qMax(10, m_settings->previewFontSize() - 1));
        m_settings->setEditorFontSize(qMax(8, m_settings->editorFontSize() - 1));
    });
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    QAction *zoomReset = view->addAction(Icons::get("zoom-original"), tr("&Reset Zoom"), this, [this] {
        m_settings->setPreviewFontSize(16);
        m_settings->setEditorFontSize(14);
    });
    zoomReset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(Icons::get("input-keyboard"), tr("&Keyboard Shortcuts"), this,
                    &MainWindow::showShortcuts);
    help->addAction(tr("Third-Party &Notices"), this, [this] {
        const QString appDir = QCoreApplication::applicationDirPath();
        for (const QString &candidate : {
                 appDir + QStringLiteral("/THIRD_PARTY_NOTICES.md"),                        // Windows
                 appDir + QStringLiteral("/../Resources/THIRD_PARTY_NOTICES.md"),           // macOS
                 appDir + QStringLiteral("/../share/doc/omnimark/THIRD_PARTY_NOTICES.md"),  // Linux
                 appDir + QStringLiteral("/usr/share/doc/omnimark/THIRD_PARTY_NOTICES.md"), // AppImage
                 QStringLiteral(OMNIMARK_SOURCE_DIR "/THIRD_PARTY_NOTICES.md"),             // dev build
             }) {
            if (QFileInfo::exists(candidate)) {
                openFile(candidate);
                return;
            }
        }
        QMessageBox::information(this, tr("Third-Party Notices"),
                                 tr("Omnimark uses Qt (LGPLv3), Chromium via QtWebEngine, and many open source "
                                    "JavaScript libraries. The notices file was not found in this installation."));
    });
    help->addAction(Icons::get("help-about"), tr("&About Omnimark"), this, &MainWindow::showAbout)
        ->setMenuRole(QAction::AboutRole);

    QToolBar *toolbar = addToolBar(tr("Main"));
    m_toolbar = toolbar;
    toolbar->setObjectName(QStringLiteral("MainToolBar"));
    toolbar->setMovable(false);
    toolbar->addAction(m_backAction);
    toolbar->addAction(m_forwardAction);
    toolbar->addSeparator();
    toolbar->addAction(toggleFiles);
    toolbar->addAction(open);
    toolbar->addAction(m_saveAction);
    toolbar->addSeparator();
    auto *modeMenu = new QMenu(this);
    modeMenu->addActions(m_viewModeGroup->actions());
    auto *editorButton = new QToolButton;
    editorButton->setDefaultAction(m_editorAction);
    editorButton->setMenu(modeMenu);
    editorButton->setPopupMode(QToolButton::MenuButtonPopup);
    editorButton->setAutoRaise(true);
    editorButton->setToolButtonStyle(toolbar->toolButtonStyle());
    editorButton->setIconSize(toolbar->iconSize());
    connect(toolbar, &QToolBar::toolButtonStyleChanged, editorButton, &QToolButton::setToolButtonStyle);
    connect(toolbar, &QToolBar::iconSizeChanged, editorButton, &QToolButton::setIconSize);
    toolbar->addWidget(editorButton);
    toolbar->addSeparator();
    toolbar->addAction(findAction);
    toolbar->addAction(toggleToc);
}

bool MainWindow::isMarkdownFile(const QString &path)
{
    static const QStringList suffixes = {
        QStringLiteral("md"), QStringLiteral("markdown"), QStringLiteral("mdown"),
        QStringLiteral("mkd"), QStringLiteral("mdx"), QStringLiteral("txt"),
    };
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

bool MainWindow::isSafeToOpen(const QString &path)
{
    const QFileInfo info(path);
    if (info.isDir())
        return true;
    if (!info.isFile() || info.isExecutable())
        return false;
    static const QStringList blocked = {
        QStringLiteral("exe"), QStringLiteral("bat"), QStringLiteral("cmd"), QStringLiteral("com"),
        QStringLiteral("msi"), QStringLiteral("msix"), QStringLiteral("ps1"), QStringLiteral("vbs"),
        QStringLiteral("vbe"), QStringLiteral("js"), QStringLiteral("jse"), QStringLiteral("wsf"),
        QStringLiteral("wsh"), QStringLiteral("scr"), QStringLiteral("lnk"), QStringLiteral("url"),
        QStringLiteral("reg"), QStringLiteral("cpl"), QStringLiteral("hta"), QStringLiteral("jar"),
        QStringLiteral("sh"), QStringLiteral("bash"), QStringLiteral("zsh"), QStringLiteral("fish"),
        QStringLiteral("command"), QStringLiteral("tool"), QStringLiteral("app"), QStringLiteral("desktop"),
        QStringLiteral("appimage"), QStringLiteral("run"), QStringLiteral("bin"), QStringLiteral("py"),
        QStringLiteral("pyw"), QStringLiteral("pl"), QStringLiteral("rb"), QStringLiteral("php"),
        QStringLiteral("apk"), QStringLiteral("deb"), QStringLiteral("rpm"), QStringLiteral("pkg"),
        QStringLiteral("dmg"), QStringLiteral("flatpakref"), QStringLiteral("workflow"), QStringLiteral("scpt"),
    };
    return !blocked.contains(info.suffix().toLower());
}

QString MainWindow::resolveDocument(const QString &path, const QStringList &searchRoots)
{
    const QFileInfo info(path);
    if (info.isFile())
        return info.absoluteFilePath();
    if (info.suffix().isEmpty() || !isMarkdownFile(path)) {
        const QFileInfo withExt(path + QStringLiteral(".md"));
        if (withExt.isFile())
            return withExt.absoluteFilePath();
    }

    // Wiki-style lookup: same file name anywhere under the search roots.
    QString wanted = info.fileName();
    if (!isMarkdownFile(wanted))
        wanted += QStringLiteral(".md");
    for (const QString &root : searchRoots) {
        if (root.isEmpty())
            continue;
        QDirIterator it(root, { QStringLiteral("*.md"), QStringLiteral("*.markdown") }, QDir::Files,
                        QDirIterator::Subdirectories);
        int visited = 0;
        while (it.hasNext() && ++visited < 20000) {
            const QFileInfo candidate = it.nextFileInfo();
            if (candidate.fileName().compare(wanted, Qt::CaseInsensitive) == 0)
                return candidate.absoluteFilePath();
        }
    }
    return {};
}

DocumentTab *MainWindow::currentTab() const
{
    return qobject_cast<DocumentTab *>(m_tabs->currentWidget());
}

QList<DocumentTab *> MainWindow::tabs() const
{
    QList<DocumentTab *> list;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *tab = qobject_cast<DocumentTab *>(m_tabs->widget(i)))
            list << tab;
    }
    return list;
}

int MainWindow::indexOf(DocumentTab *tab) const
{
    return m_tabs->indexOf(tab);
}

DocumentTab *MainWindow::createTab(const QString &path, bool activate)
{
    auto *tab = new DocumentTab(m_profile, m_roots, m_tabs);
    tab->setSettingsJson(m_settings->toJson());
    if (path.isEmpty()) {
        tab->loadUntitled();
        tab->setViewMode(DocumentTab::ViewMode::Split);
    } else {
        static const QStringList viewModes = {
            QStringLiteral("preview"), QStringLiteral("split"), QStringLiteral("source"),
        };
        tab->setViewMode(DocumentTab::ViewMode(qMax(0, viewModes.indexOf(m_settings->defaultViewMode()))));
        QString error;
        if (!tab->load(path, &error)) {
            delete tab;
            QMessageBox::warning(this, tr("Open failed"), tr("Could not open %1:\n%2").arg(path, error));
            return nullptr;
        }
    }
    tab->setTheme(currentThemeName());

    connect(tab, &DocumentTab::rendered, this, [this, tab](int ms) {
        if (tab != currentTab())
            return;
        if (ms >= 0)
            statusBar()->showMessage(tr("Rendered in %1 ms").arg(ms), 3000);
        if (m_findBar->isVisible() && !m_findBar->text().isEmpty())
            runFind(m_findBar->text(), false, m_findBar->caseSensitive(), true);
    });
    connect(tab, &DocumentTab::localFileDropped, this, [this](const QString &path) {
        if (QFileInfo(path).isDir())
            openFolder(path);
        else
            openFile(path);
    });
    connect(tab, &DocumentTab::statsChanged, this, [this, tab] {
        if (tab == currentTab())
            updateStats();
    });
    connect(tab, &DocumentTab::headingsChanged, this, [this, tab](const QVariantList &headings) {
        if (tab == currentTab())
            m_toc->setHeadings(headings);
    });
    connect(tab, &DocumentTab::activeHeadingChanged, this, [this, tab](const QString &id) {
        if (tab == currentTab())
            m_toc->setActiveHeading(id);
    });
    connect(tab, &DocumentTab::openUrlRequested, this, [this, tab](const QUrl &url, bool newTab) {
        handleUrl(tab, url, newTab);
    });
    connect(tab, &DocumentTab::fileRemoved, this, [this, tab] {
        statusBar()->showMessage(tr("%1 was removed from disk").arg(tab->path()));
    });
    connect(tab, &DocumentTab::pathChanged, this, [this, tab](const QString &newPath) {
        const int index = indexOf(tab);
        if (index < 0)
            return;
        updateTabLabel(tab);
        m_tabs->setTabToolTip(index, newPath);
        addRecentFile(newPath);
        if (tab == currentTab()) {
            updateTitle();
            m_workspace->reveal(newPath);
        }
    });
    connect(tab, &DocumentTab::historyChanged, this, &MainWindow::updateNavigation);
    connect(tab, &DocumentTab::dirtyChanged, this, [this, tab] {
        updateTabLabel(tab);
        updateNavigation();
        if (tab == currentTab())
            setWindowModified(tab->isDirty());
    });
    connect(tab, &DocumentTab::viewModeChanged, this, [this, tab] {
        if (tab == currentTab())
            updateViewModeActions();
    });
    connect(tab, &DocumentTab::externalChangeConflict, this, [this, tab] {
        m_tabs->setCurrentWidget(tab);
        QMessageBox box(QMessageBox::Warning, tr("File changed on disk"),
                        tr("%1 was changed by another program, and you have unsaved edits.").arg(tab->title()),
                        QMessageBox::NoButton, this);
        box.setInformativeText(tr("Reload the file from disk and lose your edits, or keep editing your version?"));
        QPushButton *reload = box.addButton(tr("Reload from Disk"), QMessageBox::DestructiveRole);
        box.addButton(tr("Keep My Version"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == reload)
            tab->revert();
        else
            tab->keepLocalVersion();
    });

    const int index = m_tabs->addTab(tab, tab->title());
    m_tabs->setTabToolTip(index, tab->path());
    if (!tab->isUntitled())
        addRecentFile(tab->path());
    if (activate)
        m_tabs->setCurrentIndex(index);
    return tab;
}

DocumentTab *MainWindow::openFile(const QString &path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    for (DocumentTab *tab : tabs()) {
        if (!canonical.isEmpty() && tab->path() == canonical) {
            m_tabs->setCurrentWidget(tab);
            return tab;
        }
    }
    return createTab(path, true);
}

DocumentTab *MainWindow::newDocument()
{
    DocumentTab *tab = createTab(QString(), true);
    if (tab)
        tab->evaluate(QStringLiteral("window.omnimark.focusEditor()"), [](const QVariant &) {});
    return tab;
}

DocumentTab *MainWindow::openInCurrentTab(const QString &path, const QString &fragment)
{
    DocumentTab *tab = currentTab();
    // Never navigate away from unsaved edits; use a new tab instead.
    if (!tab || tab->isDirty() || tab->isUntitled()) {
        tab = openFile(path);
        if (tab && !fragment.isEmpty())
            tab->scrollToAnchor(fragment);
        return tab;
    }
    QString error;
    if (!tab->navigate(path, fragment, &error)) {
        QMessageBox::warning(this, tr("Open failed"), tr("Could not open %1:\n%2").arg(path, error));
        return nullptr;
    }
    return tab;
}

void MainWindow::openFolder(const QString &path)
{
    if (!m_workspaceRootKey.isEmpty())
        m_roots->remove(m_workspaceRootKey);
    const QString canonical = QFileInfo(path).canonicalFilePath();
    m_workspaceRootKey = m_roots->add(canonical);
    m_workspace->setRoot(canonical);
    m_workspace->show();
}

void MainWindow::showOpenDialog()
{
    const QString start = currentTab() ? QFileInfo(currentTab()->path()).absolutePath() : m_workspace->root();
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Open Markdown"), start,
        tr("Markdown (*.md *.markdown *.mdown *.mkd *.mdx);;Text (*.txt);;All files (*)"));
    for (const QString &file : files)
        openFile(file);
}

void MainWindow::showOpenFolderDialog()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"), m_workspace->root());
    if (!dir.isEmpty())
        openFolder(dir);
}

void MainWindow::closeTab(int index)
{
    auto *tab = qobject_cast<DocumentTab *>(m_tabs->widget(index));
    if (!tab)
        return;
    QPointer<DocumentTab> guard(tab);
    confirmClose(tab, [this, guard] {
        if (!guard)
            return;
        m_tabs->removeTab(m_tabs->indexOf(guard));
        delete guard.data();
    });
}

void MainWindow::confirmClose(DocumentTab *tab, const std::function<void()> &proceed)
{
    if (!tab->isDirty()) {
        proceed();
        return;
    }
    m_tabs->setCurrentWidget(tab);
    const auto choice = QMessageBox::warning(
        this, tr("Unsaved changes"), tr("Save changes to %1 before closing?").arg(tab->title()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Discard) {
        proceed();
    } else if (choice == QMessageBox::Save) {
        saveTab(tab, [proceed](bool ok) {
            if (ok)
                proceed();
        });
    }
}

void MainWindow::saveTab(DocumentTab *tab, const std::function<void(bool)> &done)
{
    if (tab->isUntitled()) {
        saveTabAs(tab, done);
        return;
    }
    // The tab may be closed before the async save finishes; don't touch it.
    const QString path = tab->path();
    const QString title = tab->title();
    tab->save([this, path, title, done](bool ok, const QString &error) {
        if (ok)
            statusBar()->showMessage(tr("Saved %1").arg(path), 3000);
        else
            QMessageBox::critical(this, tr("Save failed"), tr("Could not save %1:\n%2").arg(title, error));
        if (done)
            done(ok);
    });
}

void MainWindow::saveTabAs(DocumentTab *tab, const std::function<void(bool)> &done)
{
    QString start = tab->path();
    if (start.isEmpty())
        start = (m_workspace->root().isEmpty() ? QDir::homePath() : m_workspace->root()) + QStringLiteral("/untitled.md");
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"), start,
                                                tr("Markdown (*.md *.markdown);;All files (*)"));
    if (path.isEmpty()) {
        if (done)
            done(false);
        return;
    }
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".md");
    tab->saveAs(path, [this, path, done](bool ok, const QString &error) {
        if (ok)
            statusBar()->showMessage(tr("Saved %1").arg(path), 3000);
        else
            QMessageBox::critical(this, tr("Save failed"), tr("Could not save %1:\n%2").arg(path, error));
        if (done)
            done(ok);
    });
}

void MainWindow::updateTabLabel(DocumentTab *tab)
{
    const int index = indexOf(tab);
    if (index < 0)
        return;
    m_tabs->setTabText(index, tab->isDirty() ? QStringLiteral("● ") + tab->title() : tab->title());
    if (tab == currentTab())
        updateTitle();
}

void MainWindow::updateViewModeActions()
{
    DocumentTab *tab = currentTab();
    const QList<QAction *> actions = m_viewModeGroup->actions();
    for (QAction *action : actions) {
        action->setEnabled(tab);
        action->setChecked(tab && action->data().toInt() == int(tab->viewMode()));
    }
    // The toggle is pressed whenever the editor is visible; its icon shows
    // which editing layout is active (split, or source only).
    const bool source = tab && tab->viewMode() == DocumentTab::ViewMode::Source;
    const bool editing = tab && tab->viewMode() != DocumentTab::ViewMode::Preview;
    m_editorAction->setEnabled(tab);
    m_editorAction->setChecked(editing);
    m_editorAction->setIcon(source ? Icons::get("document-edit", "view-source")
                                   : Icons::get("view-split-left-right", "view-split"));
    m_editorAction->setToolTip(editing ? tr("Hide the editor (%1)").arg(m_editorAction->shortcut().toString(QKeySequence::NativeText))
                                       : tr("Show the editor beside the preview (%1)").arg(m_editorAction->shortcut().toString(QKeySequence::NativeText)));
}

void MainWindow::applyToolbarStyle()
{
    const QString style = m_settings->toolbarStyle();
    m_toolbar->setToolButtonStyle(style == u"beside" ? Qt::ToolButtonTextBesideIcon
                                  : style == u"under" ? Qt::ToolButtonTextUnderIcon
                                                      : Qt::ToolButtonIconOnly);
    for (QAction *action : m_toolbarStyleGroup->actions())
        action->setChecked(action->data().toString() == style);
}

void MainWindow::setCurrentViewMode(int mode)
{
    if (DocumentTab *tab = currentTab())
        tab->setViewMode(DocumentTab::ViewMode(mode));
    updateViewModeActions();
}

QString MainWindow::currentThemeName() const
{
    switch (m_settings->theme()) {
    case Settings::Theme::Light:
        return QStringLiteral("light");
    case Settings::Theme::Dark:
        return QStringLiteral("dark");
    case Settings::Theme::System:
        break;
    }
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? QStringLiteral("dark")
                                                                                 : QStringLiteral("light");
}

void MainWindow::applySettingsToTabs()
{
    const QString json = m_settings->toJson();
    for (DocumentTab *tab : tabs())
        tab->setSettingsJson(json);
}

QString MainWindow::exportPath(const QString &title, const QString &suffix, const QString &filter)
{
    DocumentTab *tab = currentTab();
    QString start = tab && !tab->isUntitled() ? tab->path() : QDir::homePath() + QStringLiteral("/untitled.md");
    start = QFileInfo(start).absolutePath() + u'/' + QFileInfo(start).completeBaseName() + u'.' + suffix;
    QString path = QFileDialog::getSaveFileName(this, title, start, filter);
    if (!path.isEmpty() && QFileInfo(path).suffix().isEmpty())
        path += u'.' + suffix;
    return path;
}

void MainWindow::exportPdf()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    const QString path = exportPath(tr("Export as PDF"), QStringLiteral("pdf"), tr("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    const QPageSize size(QLocale().measurementSystem() == QLocale::ImperialUSSystem ? QPageSize::Letter
                                                                                    : QPageSize::A4);
    statusBar()->showMessage(tr("Exporting PDF…"));
    m_exporter->exportPdf(tab, path, size, QMarginsF(15, 15, 15, 15), [this, path](bool ok, const QString &error) {
        if (ok)
            statusBar()->showMessage(tr("Exported %1").arg(path), 5000);
        else
            QMessageBox::critical(this, tr("Export failed"), error);
    });
}

void MainWindow::exportHtml()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    const QString path = exportPath(tr("Export as HTML"), QStringLiteral("html"), tr("HTML (*.html *.htm)"));
    if (path.isEmpty())
        return;
    statusBar()->showMessage(tr("Exporting HTML…"));
    m_exporter->exportHtml(tab, path, true, [this, path](bool ok, const QString &error) {
        if (ok)
            statusBar()->showMessage(tr("Exported %1").arg(path), 5000);
        else
            QMessageBox::critical(this, tr("Export failed"), error);
    });
}

void MainWindow::showShortcuts()
{
    QString rows;
    const QList<QMenu *> menus = menuBar()->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly);
    for (QMenu *menu : menus) {
        bool header = false;
        for (QAction *action : menu->actions()) {
            if (action->shortcuts().isEmpty() || action->isSeparator())
                continue;
            if (!header) {
                rows += QStringLiteral("<tr><th colspan=2 align=left><br>%1</th></tr>")
                            .arg(menu->title().remove(u'&'));
                header = true;
            }
            QStringList keys;
            for (const QKeySequence &key : action->shortcuts())
                keys << key.toString(QKeySequence::NativeText).toHtmlEscaped();
            rows += QStringLiteral("<tr><td>%1</td><td>&nbsp;&nbsp;<b>%2</b></td></tr>")
                        .arg(action->text().remove(u'&').remove(QStringLiteral("…")).toHtmlEscaped(),
                             keys.join(QStringLiteral(", ")));
        }
    }
    QMessageBox box(this);
    box.setWindowTitle(tr("Keyboard Shortcuts"));
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral("<table>%1</table>").arg(rows)
                + tr("<p><b>In the editor</b><br>"
                     "Ctrl+B bold · Ctrl+I italic · Ctrl+K link · Ctrl+Shift+C inline code · "
                     "Ctrl+Shift+X strikethrough<br>"
                     "Ctrl+F find / replace · Ctrl+Z / Ctrl+Shift+Z undo / redo<br>"
                     "Paste or drop an image to save it next to the document and link it.</p>"));
    box.exec();
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About Omnimark"),
                       tr("<h3>Omnimark %1</h3>"
                          "<p>A Markdown viewer and editor.</p>"
                          "<p>Built with Qt %2 and QtWebEngine; rendering by unified/remark/rehype, "
                          "Shiki, KaTeX, Mermaid, Graphviz, Vega, abcjs, WaveDrom and nomnoml.</p>")
                           .arg(QStringLiteral(OMNIMARK_VERSION), QString::fromLatin1(qVersion())));
}

void MainWindow::updateStats()
{
    DocumentTab *tab = currentTab();
    const int words = tab ? tab->wordCount() : 0;
    if (!tab || words <= 0) {
        m_statsLabel->clear();
        return;
    }
    const int minutes = qMax(1, (words + 229) / 230);
    const QString count = QLocale().toString(words);
    m_statsLabel->setText((words == 1 ? tr("1 word") : tr("%1 words").arg(count))
                          + tr(" · %1 min read").arg(minutes));
}

void MainWindow::syncDocks()
{
    DocumentTab *tab = currentTab();
    m_toc->setHeadings(tab ? tab->headings() : QVariantList());
    if (m_findBar->isVisible() && tab && !m_findBar->text().isEmpty())
        runFind(m_findBar->text(), false, m_findBar->caseSensitive(), true);
}

void MainWindow::startFind()
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    QPointer<DocumentTab> guard(tab);
    tab->evaluate(QStringLiteral("[window.omnimark.findTarget(), window.omnimark.selectedText()]"),
                  [this, guard](const QVariant &result) {
        if (!guard)
            return;
        const QVariantList list = result.toList();
        if (list.value(0).toString() == u"editor") {
            m_findBar->hide();
            return;
        }
        m_findBar->activate(list.value(1).toString().section(u'\n', 0, 0));
        if (!m_findBar->text().isEmpty())
            runFind(m_findBar->text(), false, m_findBar->caseSensitive(), true);
    });
}

void MainWindow::runFind(const QString &text, bool backward, bool caseSensitive, bool refresh)
{
    DocumentTab *tab = currentTab();
    if (!tab)
        return;
    const QString script = QStringLiteral("window.omnimark.find(%1, %2, %3, %4)")
                               .arg(QString::fromUtf8(QJsonDocument(QJsonArray { text }).toJson(QJsonDocument::Compact)).mid(1).chopped(1),
                                    backward ? u"true"_s : u"false"_s, caseSensitive ? u"true"_s : u"false"_s,
                                    refresh ? u"true"_s : u"false"_s);
    tab->evaluate(script, [this](const QVariant &result) {
        const QVariantMap map = result.toMap();
        m_findBar->setResult(map.value(QStringLiteral("active")).toInt(), map.value(QStringLiteral("total")).toInt());
    });
}

void MainWindow::applyThemeToTabs()
{
    const QString theme = currentThemeName();
    for (DocumentTab *tab : tabs())
        tab->setTheme(theme);
}

void MainWindow::handleUrl(DocumentTab *source, const QUrl &url, bool newTab)
{
    if (url.scheme() == u"omnimark" && url.host() == u"local") {
        QString path = url.path(QUrl::FullyDecoded);
#ifdef Q_OS_WIN
        if (path.size() > 2 && path.at(0) == u'/' && path.at(2) == u':')
            path.remove(0, 1);
#endif
        QStringList roots = { m_workspace->root() };
        if (source)
            roots << QFileInfo(source->path()).absolutePath();
        const QString resolved = resolveDocument(path, roots);
        if (!resolved.isEmpty() && isMarkdownFile(resolved)) {
            // Opening a document grants the page access to its folder, so
            // documents outside the opened folders need the user's say-so.
            if (!m_roots->isAllowed(resolved)
                && QMessageBox::question(this, tr("Open linked file?"),
                                         tr("This link opens a file outside the folders you have open:\n\n%1\n\n"
                                            "Open it?").arg(QDir::toNativeSeparators(resolved)))
                       != QMessageBox::Yes) {
                return;
            }
            if (newTab || !source) {
                DocumentTab *tab = openFile(resolved);
                if (tab && url.hasFragment())
                    tab->scrollToAnchor(url.fragment(QUrl::FullyDecoded));
            } else {
                m_tabs->setCurrentWidget(source);
                openInCurrentTab(resolved, url.fragment(QUrl::FullyDecoded));
            }
        } else if (!m_roots->isAllowed(path) && !m_roots->containsDirectory(path)) {
            // Don't reveal whether paths outside the open folders exist.
            statusBar()->showMessage(tr("Links to files outside the open folders can't be opened"), 5000);
        } else if (!isSafeToOpen(path)) {
            statusBar()->showMessage(tr("Not opening %1: programs and scripts are never launched from documents")
                                         .arg(QFileInfo(path).fileName()), 6000);
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
        return;
    }

    static const QStringList external = {
        QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("mailto"),
    };
    if (external.contains(url.scheme()))
        QDesktopServices::openUrl(url);
}

void MainWindow::updateTitle()
{
    DocumentTab *tab = currentTab();
    setWindowTitle(tab ? tr("%1[*] — Omnimark").arg(tab->title()) : tr("Omnimark"));
    setWindowFilePath(tab ? tab->path() : QString());
    setWindowModified(tab && tab->isDirty());
}

void MainWindow::updateNavigation()
{
    DocumentTab *tab = currentTab();
    const bool clean = tab && !tab->isDirty();
    m_backAction->setEnabled(clean && tab->canGoBack());
    m_forwardAction->setEnabled(clean && tab->canGoForward());
    m_saveAction->setEnabled(tab);
    m_saveAsAction->setEnabled(tab);
    m_revertAction->setEnabled(tab && tab->isDirty() && !tab->isUntitled());
}

void MainWindow::addRecentFile(const QString &path)
{
    if (path.isEmpty())
        return;
    QSettings settings;
    QStringList recent = settings.value(QStringLiteral("recentFiles")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > MaxRecentFiles)
        recent.removeLast();
    settings.setValue(QStringLiteral("recentFiles"), recent);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    m_recentMenu->clear();
    const QStringList recent = QSettings().value(QStringLiteral("recentFiles")).toStringList();
    for (const QString &path : recent) {
        if (!QFileInfo::exists(path))
            continue;
        QAction *action = m_recentMenu->addAction(QFileInfo(path).fileName(), this, [this, path] {
            openFile(path);
        });
        action->setToolTip(path);
        action->setStatusTip(path);
    }
    if (m_recentMenu->isEmpty()) {
        m_recentMenu->addAction(tr("No recent files"))->setEnabled(false);
        return;
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction(tr("Clear Recent Files"), this, [this] {
        QSettings().remove(QStringLiteral("recentFiles"));
        rebuildRecentMenu();
    });
}

void MainWindow::saveSession() const
{
    QSettings settings;
    QStringList files;
    QVariantList modes;
    int current = 0;
    for (DocumentTab *tab : tabs()) {
        if (tab->isUntitled())
            continue;
        if (tab == currentTab())
            current = int(files.size());
        files << tab->path();
        modes << int(tab->viewMode());
    }
    settings.setValue(QStringLiteral("session/modes"), modes);
    settings.setValue(QStringLiteral("session/files"), files);
    settings.setValue(QStringLiteral("session/current"), current);
    settings.setValue(QStringLiteral("session/workspace"), m_workspace->root());
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
}

void MainWindow::restoreSession()
{
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    const QString workspace = settings.value(QStringLiteral("session/workspace")).toString();
    if (!workspace.isEmpty() && QFileInfo(workspace).isDir())
        openFolder(workspace);
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
    if (workspace.isEmpty())
        m_workspace->hide();

    const QStringList files = settings.value(QStringLiteral("session/files")).toStringList();
    const QVariantList modes = settings.value(QStringLiteral("session/modes")).toList();
    const int current = settings.value(QStringLiteral("session/current"), 0).toInt();
    DocumentTab *currentRestored = nullptr;
    for (int i = 0; i < files.size(); ++i) {
        if (!QFileInfo(files[i]).isFile())
            continue;
        DocumentTab *tab = createTab(files[i], false);
        if (tab && i < modes.size())
            tab->setViewMode(DocumentTab::ViewMode(qBound(0, modes[i].toInt(), 2)));
        if (tab && (i == current || !currentRestored))
            currentRestored = tab;
    }
    if (currentRestored)
        m_tabs->setCurrentWidget(currentRestored);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QList<DocumentTab *> dirty;
    for (DocumentTab *tab : tabs()) {
        if (tab->isDirty())
            dirty << tab;
    }
    if (m_closing || dirty.isEmpty()) {
        saveSession();
        event->accept();
        return;
    }

    event->ignore();
    QStringList names;
    for (DocumentTab *tab : dirty)
        names << tab->title();
    QMessageBox box(QMessageBox::Warning, tr("Unsaved changes"),
                    dirty.size() == 1 ? tr("1 document has unsaved changes.")
                                      : tr("%1 documents have unsaved changes.").arg(dirty.size()),
                    QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.setInformativeText(names.join(u'\n'));
    box.setDefaultButton(QMessageBox::SaveAll);
    const int choice = box.exec();
    if (choice == QMessageBox::Discard) {
        m_closing = true;
        close();
    } else if (choice == QMessageBox::SaveAll) {
        auto remaining = std::make_shared<int>(int(dirty.size()));
        auto failed = std::make_shared<bool>(false);
        for (DocumentTab *tab : dirty) {
            saveTab(tab, [this, remaining, failed](bool ok) {
                *failed = *failed || !ok;
                if (--*remaining == 0 && !*failed) {
                    m_closing = true;
                    close();
                }
            });
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    for (const QUrl &url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (QFileInfo(path).isDir())
            openFolder(path);
        else
            openFile(path);
    }
}

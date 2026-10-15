// End-to-end checks of the real window with QtWebEngine pages.
#include "DocumentTab.h"
#include "Exporter.h"
#include "LocalRoots.h"
#include "MainWindow.h"
#include "SchemeHandler.h"
#include "Settings.h"
#include "Bridge.h"
#include "SettingsDialog.h"
#include "WebAssets.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QMimeData>
#include <QDropEvent>
#include <QWebEngineView>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWebEngineProfile>

class TestMainWindow : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void rendersAndReportsHeadings();
    void editMarksDirtyAndSaveWrites();
    void externalChangeWhileDirtyAsks();
    void externalChangeWhenCleanReloads();
    void wikiLinkNavigatesInTab();
    void secondOpenFocusesExistingTab();
    void findInPreviewCountsAndCycles();
    void scrollToLineAfterOpen();
    void exportsPdfAndHtml();
    void settingsReachThePage();
    void sessionRoundTrip();
    void neverLaunchesProgramsFromLinks();
    void asksBeforeOpeningLinkedFileOutsideRoots();
    void fileDropOnViewOpensTab();
    void grabDialogs();
    void savesPastedImagesNextToDocument();
    void editorToggleFollowsViewMode();
    void droppedImageInEditorInsertsLink();

private:
    static QVariant js(DocumentTab *tab, const QString &script);
    static void writeFile(const QString &path, const QByteArray &data);
    static QByteArray readFile(const QString &path);
    DocumentTab *openAndRender(const QString &path);
    // Answers the next modal message box with `button`.
    void answerNextMessageBox(QMessageBox::StandardButton button, const QString &customText = {});

    LocalRoots m_roots;
    QWebEngineProfile *m_profile = nullptr;
    SchemeHandler *m_handler = nullptr;
    Settings *m_settings = nullptr;
    MainWindow *m_window = nullptr;
    QTemporaryDir m_dir;
};

void TestMainWindow::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QSettings().clear();
    const QString web = WebAssets::directory();
    if (web.isEmpty())
        QSKIP("web frontend not built");
    m_settings = new Settings(this);
    m_profile = new QWebEngineProfile(this);
    m_handler = new SchemeHandler(web, &m_roots, this);
    m_profile->installUrlSchemeHandler(SchemeHandler::Scheme, m_handler);
}

void TestMainWindow::init()
{
    m_window = new MainWindow(m_profile, &m_roots, m_settings);
    m_window->resize(1000, 700);
    m_window->show();
}

void TestMainWindow::cleanup()
{
    // Discard edits so the close guard doesn't block.
    for (DocumentTab *tab : m_window->tabs()) {
        if (tab->isDirty())
            tab->revert();
    }
    QTRY_VERIFY_WITH_TIMEOUT([this] {
        for (DocumentTab *tab : m_window->tabs())
            if (tab->isDirty())
                return false;
        return true;
    }(), 5000);
    delete m_window;
    m_window = nullptr;
}

QVariant TestMainWindow::js(DocumentTab *tab, const QString &script)
{
    QVariant result;
    bool done = false;
    tab->evaluate(script, [&](const QVariant &value) {
        result = value;
        done = true;
    });
    if (!QTest::qWaitFor([&] { return done; }, 5000))
        qWarning() << "js() timed out:" << script;
    return result;
}

void TestMainWindow::writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

QByteArray TestMainWindow::readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

DocumentTab *TestMainWindow::openAndRender(const QString &path)
{
    DocumentTab *tab = m_window->openFile(path);
    if (!tab)
        return nullptr;
    QSignalSpy rendered(tab, &DocumentTab::rendered);
    if (!rendered.wait(20000))
        return nullptr;
    return tab;
}

void TestMainWindow::answerNextMessageBox(QMessageBox::StandardButton button, const QString &customText)
{
    QTimer::singleShot(0, this, [this, button, customText] {
        auto attempt = std::make_shared<std::function<void()>>();
        *attempt = [button, customText, attempt] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) {
                QTimer::singleShot(50, *attempt);
                return;
            }
            if (!customText.isEmpty()) {
                for (QAbstractButton *b : box->buttons()) {
                    if (b->text() == customText) {
                        b->click();
                        return;
                    }
                }
            }
            box->button(button)->click();
        };
        (*attempt)();
    });
}

void TestMainWindow::rendersAndReportsHeadings()
{
    const QString path = m_dir.filePath(QStringLiteral("a.md"));
    writeFile(path, "# Alpha\n\n## Beta\n\n```mermaid\ngraph TD; A-->B\n```\n");
    DocumentTab *tab = openAndRender(path);
    QVERIFY(tab);
    QTRY_COMPARE_WITH_TIMEOUT(tab->headings().size(), 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(js(tab, QStringLiteral("!!document.querySelector('.om-fence svg')")).toBool(), 15000);
}

void TestMainWindow::editMarksDirtyAndSaveWrites()
{
    const QString path = m_dir.filePath(QStringLiteral("edit.md"));
    writeFile(path, "# Original\n");
    DocumentTab *tab = openAndRender(path);
    QVERIFY(tab);
    tab->setViewMode(DocumentTab::ViewMode::Split);

    QSignalSpy dirty(tab, &DocumentTab::dirtyChanged);
    js(tab, QStringLiteral("window.omnimark.userEdit('# Edited\\n\\nnew text\\n')"));
    QTRY_VERIFY_WITH_TIMEOUT(tab->isDirty(), 5000);
    QVERIFY(m_window->isWindowModified());

    QTRY_COMPARE_WITH_TIMEOUT(js(tab, QStringLiteral("document.querySelector('#preview h1').textContent")).toString(),
                              QStringLiteral("Edited"), 5000);

    // Real shortcut through the window.
    m_window->activateWindow();
    QTest::keySequence(m_window, QKeySequence::Save);
    QTRY_VERIFY_WITH_TIMEOUT(!tab->isDirty(), 5000);
    QCOMPARE(readFile(path), QByteArray("# Edited\n\nnew text\n"));

    // Our own save must not be treated as an external change.
    QTest::qWait(600);
    QVERIFY(!tab->isDirty());
}

void TestMainWindow::externalChangeWhileDirtyAsks()
{
    const QString path = m_dir.filePath(QStringLiteral("conflict.md"));
    writeFile(path, "# Disk v1\n");
    DocumentTab *tab = openAndRender(path);
    QVERIFY(tab);
    js(tab, QStringLiteral("window.omnimark.userEdit('# Mine\\n')"));
    QTRY_VERIFY_WITH_TIMEOUT(tab->isDirty(), 5000);

    QSignalSpy conflict(tab, &DocumentTab::externalChangeConflict);
    answerNextMessageBox(QMessageBox::NoButton, QStringLiteral("Keep My Version"));
    writeFile(path, "# Disk v2\n");
    QTRY_COMPARE_WITH_TIMEOUT(conflict.count(), 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!QApplication::activeModalWidget(), 5000);
    QVERIFY(tab->isDirty());
    QCOMPARE(js(tab, QStringLiteral("window.omnimark.getText()")).toString(), QStringLiteral("# Mine\n"));

    // Second conflict: choose reload.
    answerNextMessageBox(QMessageBox::NoButton, QStringLiteral("Reload from Disk"));
    writeFile(path, "# Disk v3\n");
    QTRY_COMPARE_WITH_TIMEOUT(js(tab, QStringLiteral("window.omnimark.getText()")).toString(),
                              QStringLiteral("# Disk v3\n"), 8000);
    QTRY_VERIFY_WITH_TIMEOUT(!tab->isDirty(), 5000);
}

void TestMainWindow::externalChangeWhenCleanReloads()
{
    const QString path = m_dir.filePath(QStringLiteral("clean.md"));
    writeFile(path, "# One\n");
    DocumentTab *tab = openAndRender(path);
    QVERIFY(tab);
    writeFile(path, "# Two\n");
    QTRY_COMPARE_WITH_TIMEOUT(js(tab, QStringLiteral("document.querySelector('#preview h1').textContent")).toString(),
                              QStringLiteral("Two"), 5000);
}

void TestMainWindow::wikiLinkNavigatesInTab()
{
    QDir(m_dir.path()).mkpath(QStringLiteral("wiki/sub"));
    const QString index = m_dir.filePath(QStringLiteral("wiki/index.md"));
    writeFile(index, "# Index\n\n[[Target Page#Part Two]]\n");
    writeFile(m_dir.filePath(QStringLiteral("wiki/sub/Target Page.md")), "# Target\n\n## Part Two\n");
    m_window->openFolder(m_dir.filePath(QStringLiteral("wiki")));
    DocumentTab *tab = openAndRender(index);
    QVERIFY(tab);
    const int tabsBefore = m_window->tabs().size();

    QSignalSpy history(tab, &DocumentTab::historyChanged);
    js(tab, QStringLiteral("document.querySelector('a.wikilink').click()"));
    // The signal may fire while js() is still pumping events, so poll.
    QTRY_VERIFY_WITH_TIMEOUT(history.count() > 0, 5000);
    QCOMPARE(m_window->tabs().size(), tabsBefore);
    QVERIFY(tab->path().endsWith(QStringLiteral("sub/Target Page.md")));
    QVERIFY(tab->canGoBack());
    tab->goBack();
    QVERIFY(tab->path().endsWith(QStringLiteral("wiki/index.md")));
}

void TestMainWindow::secondOpenFocusesExistingTab()
{
    const QString path = m_dir.filePath(QStringLiteral("once.md"));
    writeFile(path, "# Once\n");
    DocumentTab *first = m_window->openFile(path);
    DocumentTab *second = m_window->openFile(path);
    QCOMPARE(first, second);
    QCOMPARE(m_window->tabs().size(), 1);
}

void TestMainWindow::findInPreviewCountsAndCycles()
{
    const QString path = m_dir.filePath(QStringLiteral("find.md"));
    writeFile(path, "# Apple\n\napple pie and APPLE juice\n\n```js\nconst apple = 1;\n```\n");
    DocumentTab *tab = openAndRender(path);
    QVERIFY(tab);
    QVariantMap r = js(tab, QStringLiteral("window.omnimark.find('apple', false, false)")).toMap();
    QCOMPARE(r.value(QStringLiteral("total")).toInt(), 4);
    QCOMPARE(r.value(QStringLiteral("active")).toInt(), 1);
    r = js(tab, QStringLiteral("window.omnimark.find('apple', false, false)")).toMap();
    QCOMPARE(r.value(QStringLiteral("active")).toInt(), 2);
    r = js(tab, QStringLiteral("window.omnimark.find('apple', true, false)")).toMap();
    QCOMPARE(r.value(QStringLiteral("active")).toInt(), 1);
    r = js(tab, QStringLiteral("window.omnimark.find('apple', true, false)")).toMap();
    QCOMPARE(r.value(QStringLiteral("active")).toInt(), 4); // wraps
    r = js(tab, QStringLiteral("window.omnimark.find('APPLE', false, true)")).toMap();
    QCOMPARE(r.value(QStringLiteral("total")).toInt(), 1);
    QVERIFY(js(tab, QStringLiteral("CSS.highlights.has('om-find')")).toBool());
    js(tab, QStringLiteral("window.omnimark.clearFind()"));
    QVERIFY(!js(tab, QStringLiteral("CSS.highlights.has('om-find')")).toBool());
}

void TestMainWindow::scrollToLineAfterOpen()
{
    QByteArray text = "# Top\n\n";
    for (int i = 0; i < 200; ++i)
        text += "Paragraph " + QByteArray::number(i) + "\n\n";
    text += "## Bottom\n";
    const QString path = m_dir.filePath(QStringLiteral("long.md"));
    writeFile(path, text);
    DocumentTab *tab = m_window->openFile(path);
    tab->scrollToLine(400); // queued until the first render
    QTRY_VERIFY_WITH_TIMEOUT(js(tab, QStringLiteral("document.getElementById('preview-pane').scrollTop")).toDouble() > 1000,
                             10000);
}

void TestMainWindow::exportsPdfAndHtml()
{
    QFile::copy(QStringLiteral(OMNIMARK_SOURCE_DIR "/samples/img/logo.svg"), m_dir.filePath(QStringLiteral("logo.svg")));
    const QString path = m_dir.filePath(QStringLiteral("export.md"));
    writeFile(path, "# Export me\n\n![logo](logo.svg)\n\n$x^2$\n\n```dot\ndigraph { a -> b }\n```\n");
    DocumentTab *tab = openAndRender(path);
    QVERIFY(tab);
    tab->setTheme(QStringLiteral("dark"));
    QTRY_VERIFY_WITH_TIMEOUT(js(tab, QStringLiteral("!!document.querySelector('.om-fence svg')")).toBool(), 15000);

    Exporter exporter;
    bool done = false;
    bool ok = false;
    const QString pdf = m_dir.filePath(QStringLiteral("out.pdf"));
    exporter.exportPdf(tab, pdf, QPageSize(QPageSize::A4), QMarginsF(10, 10, 10, 10), [&](bool success, const QString &) {
        ok = success;
        done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    QVERIFY(ok);
    QVERIFY(readFile(pdf).startsWith("%PDF"));
    QCOMPARE(tab->theme(), QStringLiteral("dark")); // restored after export

    done = ok = false;
    const QString html = m_dir.filePath(QStringLiteral("out.html"));
    exporter.exportHtml(tab, html, true, [&](bool success, const QString &) {
        ok = success;
        done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    QVERIFY(ok);
    const QByteArray out = readFile(html);
    QVERIFY(out.startsWith("<!doctype html>"));
    QVERIFY(out.contains("<h1"));
    QVERIFY(out.contains("Export me"));
    QVERIFY(out.contains("class=\"katex\""));
    QVERIFY(out.contains("<svg"));
    QVERIFY(out.contains("src=\"data:image/svg+xml"));     // local image inlined
    QVERIFY(out.contains("url(\"data:font/"));             // KaTeX fonts inlined
    QVERIFY(!out.contains("om-fence-toolbar\""));
    QVERIFY(!out.contains("omnimark://"));
}

void TestMainWindow::settingsReachThePage()
{
    const QString path = m_dir.filePath(QStringLiteral("settings.md"));
    writeFile(path, "# S\n\n![remote](https://example.com/x.png)\n");
    DocumentTab *tab = openAndRender(path);
    QVERIFY(tab);
    m_settings->setPreviewStyle(QStringLiteral("serif"));
    m_settings->setRemoteImages(false);
    QTRY_COMPARE_WITH_TIMEOUT(js(tab, QStringLiteral("document.documentElement.dataset.style")).toString(),
                              QStringLiteral("serif"), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(js(tab, QStringLiteral("!!document.querySelector('a.om-remote-image')")).toBool(), 5000);
    m_settings->setTheme(Settings::Theme::Dark);
    QTRY_COMPARE_WITH_TIMEOUT(js(tab, QStringLiteral("document.documentElement.dataset.theme")).toString(),
                              QStringLiteral("dark"), 5000);
    // Restore defaults for other tests.
    m_settings->setPreviewStyle(QStringLiteral("github"));
    m_settings->setRemoteImages(true);
    m_settings->setTheme(Settings::Theme::System);
}

void TestMainWindow::sessionRoundTrip()
{
    const QString a = m_dir.filePath(QStringLiteral("session-a.md"));
    const QString b = m_dir.filePath(QStringLiteral("session-b.md"));
    writeFile(a, "# A\n");
    writeFile(b, "# B\n");
    QDir(m_dir.path()).mkpath(QStringLiteral("ws"));
    m_window->openFolder(m_dir.filePath(QStringLiteral("ws")));
    m_window->openFile(a);
    DocumentTab *second = m_window->openFile(b);
    second->setViewMode(DocumentTab::ViewMode::Split);
    m_window->newDocument(); // untitled tabs are not restored
    QVERIFY(m_window->close());

    delete m_window;
    m_window = new MainWindow(m_profile, &m_roots, m_settings);
    m_window->restoreSession();
    m_window->show();
    const QList<DocumentTab *> tabs = m_window->tabs();
    QCOMPARE(tabs.size(), 2);
    QCOMPARE(tabs[0]->path(), QFileInfo(a).canonicalFilePath());
    QCOMPARE(tabs[1]->path(), QFileInfo(b).canonicalFilePath());
    QCOMPARE(tabs[1]->viewMode(), DocumentTab::ViewMode::Split);
    QVERIFY(QSettings().value(QStringLiteral("recentFiles")).toStringList().contains(QFileInfo(b).canonicalFilePath()));
    QCOMPARE(QSettings().value(QStringLiteral("session/workspace")).toString(),
             QFileInfo(m_dir.filePath(QStringLiteral("ws"))).canonicalFilePath());
}

void TestMainWindow::neverLaunchesProgramsFromLinks()
{
    QVERIFY(!MainWindow::isSafeToOpen(m_dir.filePath(QStringLiteral("missing.exe"))));
    const QString script = m_dir.filePath(QStringLiteral("setup.sh"));
    const QString exe = m_dir.filePath(QStringLiteral("setup.exe"));
    const QString pdf = m_dir.filePath(QStringLiteral("paper.pdf"));
    for (const QString &p : { script, exe, pdf })
        writeFile(p, "x");
    QFile::setPermissions(script, QFile::permissions(script) | QFile::ExeOwner);
    QVERIFY(!MainWindow::isSafeToOpen(script));
    QVERIFY(!MainWindow::isSafeToOpen(exe));
    QVERIFY(MainWindow::isSafeToOpen(pdf));
    QVERIFY(MainWindow::isSafeToOpen(m_dir.path()));

    const QString doc = m_dir.filePath(QStringLiteral("links.md"));
    writeFile(doc, "[run](setup.exe)\n");
    DocumentTab *tab = openAndRender(doc);
    QVERIFY(tab);
    js(tab, QStringLiteral("document.querySelector('#preview a').click()"));
    QTRY_VERIFY_WITH_TIMEOUT(m_window->statusBar()->currentMessage().contains(QStringLiteral("never launched")), 5000);
}

void TestMainWindow::asksBeforeOpeningLinkedFileOutsideRoots()
{
    QDir(m_dir.path()).mkpath(QStringLiteral("inside"));
    QDir(m_dir.path()).mkpath(QStringLiteral("outside"));
    const QString outsideDoc = m_dir.filePath(QStringLiteral("outside/secret.md"));
    writeFile(outsideDoc, "# Secret\n");
    const QString doc = m_dir.filePath(QStringLiteral("inside/index.md"));
    writeFile(doc, "[x](" + outsideDoc.toUtf8() + ")\n");
    DocumentTab *tab = openAndRender(doc);
    QVERIFY(tab);

    answerNextMessageBox(QMessageBox::No);
    js(tab, QStringLiteral("document.querySelector('#preview a').click()"));
    QTRY_VERIFY_WITH_TIMEOUT(!QApplication::activeModalWidget(), 5000);
    QTest::qWait(200);
    QVERIFY(tab->path().endsWith(QStringLiteral("inside/index.md")));
    QVERIFY(!m_roots.isAllowed(outsideDoc));

    answerNextMessageBox(QMessageBox::Yes);
    js(tab, QStringLiteral("document.querySelector('#preview a').click()"));
    QTRY_VERIFY_WITH_TIMEOUT(tab->path().endsWith(QStringLiteral("outside/secret.md")), 5000);
}

void TestMainWindow::fileDropOnViewOpensTab()
{
    const QString a = m_dir.filePath(QStringLiteral("drop-a.md"));
    const QString b = m_dir.filePath(QStringLiteral("drop-b.md"));
    writeFile(a, "# A\n");
    writeFile(b, "# B\n");
    DocumentTab *tab = openAndRender(a);
    QVERIFY(tab);
    QMimeData mime;
    mime.setUrls({ QUrl::fromLocalFile(b) });
    QWidget *target = tab->view()->focusProxy() ? tab->view()->focusProxy() : tab->view();
    QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    QTRY_COMPARE_WITH_TIMEOUT(m_window->tabs().size(), 2, 5000);
    QCOMPARE(m_window->currentTab()->path(), QFileInfo(b).canonicalFilePath());
}

void TestMainWindow::editorToggleFollowsViewMode()
{
    const QString path = m_dir.filePath(QStringLiteral("toggle.md"));
    writeFile(path, "# T\n");
    DocumentTab *tab = openAndRender(path);
    QVERIFY(tab);
    auto *toggle = m_window->findChild<QAction *>(QStringLiteral("editorToggle"));
    QVERIFY(toggle);
    QCOMPARE(tab->viewMode(), DocumentTab::ViewMode::Preview);
    QVERIFY(!toggle->isChecked());

    toggle->trigger();
    QCOMPARE(tab->viewMode(), DocumentTab::ViewMode::Split);
    QVERIFY(toggle->isChecked());
    toggle->trigger();
    QCOMPARE(tab->viewMode(), DocumentTab::ViewMode::Preview);

    tab->setViewMode(DocumentTab::ViewMode::Source);
    QVERIFY(toggle->isChecked()); // pressed: the editor is showing
    toggle->trigger();            // hides it
    QCOMPARE(tab->viewMode(), DocumentTab::ViewMode::Preview);
}

void TestMainWindow::savesPastedImagesNextToDocument()
{
    Bridge bridge;
    QVERIFY(bridge.saveImage(QStringLiteral("aGVsbG8="), QStringLiteral("image/png")).isEmpty()); // no document yet
    QDir(m_dir.path()).mkpath(QStringLiteral("paste"));
    bridge.setDocumentDirectory(m_dir.filePath(QStringLiteral("paste")));
    QVERIFY(bridge.saveImage(QStringLiteral("aGVsbG8="), QStringLiteral("text/html")).isEmpty()); // not an image
    const QString relative = bridge.saveImage(QStringLiteral("aGVsbG8="), QStringLiteral("image/jpeg"));
    QVERIFY(relative.startsWith(QStringLiteral("images/pasted-")));
    QVERIFY(relative.endsWith(QStringLiteral(".jpg")));
    QCOMPARE(readFile(m_dir.filePath(QStringLiteral("paste/") + relative)), QByteArray("hello"));
}

void TestMainWindow::droppedImageInEditorInsertsLink()
{
    QDir(m_dir.path()).mkpath(QStringLiteral("dropimg"));
    QDir(m_dir.path()).mkpath(QStringLiteral("elsewhere"));
    const QString doc = m_dir.filePath(QStringLiteral("dropimg/doc.md"));
    writeFile(doc, "");
    const QString inside = m_dir.filePath(QStringLiteral("dropimg/local pic.png"));
    const QString outside = m_dir.filePath(QStringLiteral("elsewhere/far.png"));
    writeFile(inside, "png");
    writeFile(outside, "png");
    DocumentTab *tab = openAndRender(doc);
    QVERIFY(tab);
    tab->setViewMode(DocumentTab::ViewMode::Split);

    QMimeData mime;
    mime.setUrls({ QUrl::fromLocalFile(inside), QUrl::fromLocalFile(outside) });
    QWidget *target = tab->view()->focusProxy() ? tab->view()->focusProxy() : tab->view();
    QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    QVERIFY(drop.isAccepted());
    QTRY_COMPARE_WITH_TIMEOUT(js(tab, QStringLiteral("window.omnimark.getText()")).toString(),
                              QStringLiteral("![local pic](local%20pic.png)\n![far](images/far.png)"), 5000);
    QVERIFY(QFileInfo::exists(m_dir.filePath(QStringLiteral("dropimg/images/far.png")))); // copied in
    QCOMPARE(m_window->tabs().size(), 1); // images don't open as tabs in edit modes
}

// Visual check: OMNIMARK_GRAB_DIR=/some/dir saves screenshots of dialogs there.
void TestMainWindow::grabDialogs()
{
    const QString dir = qEnvironmentVariable("OMNIMARK_GRAB_DIR");
    if (dir.isEmpty())
        QSKIP("set OMNIMARK_GRAB_DIR to save dialog screenshots");
    SettingsDialog dialog(m_settings, m_window);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QVERIFY(dialog.grab().save(dir + QStringLiteral("/settings-dialog.png")));
}

int main(int argc, char **argv)
{
    SchemeHandler::registerScheme();
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("OmnimarkTest"));
    QApplication::setApplicationName(QStringLiteral("OmnimarkTest"));
    TestMainWindow test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_mainwindow.moc"

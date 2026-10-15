#include "DocumentTab.h"

#include "Bridge.h"
#include "FileWatcher.h"
#include "LocalRoots.h"
#include "WebAssets.h"
#include "WebPage.h"

#include <QApplication>
#include <QPointer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QChildEvent>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QWebEngineContextMenuRequest>
#include <QSaveFile>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEngineSettings>
#include <QWebEngineView>

QUrl DocumentTab::appUrl()
{
    const QString dev = WebAssets::devServerUrl();
    if (!dev.isEmpty())
        return QUrl(dev + QStringLiteral("/index.html"));
    return QUrl(QStringLiteral("omnimark://app/index.html"));
}

DocumentTab::DocumentTab(QWebEngineProfile *profile, LocalRoots *roots, QWidget *parent)
    : QWidget(parent)
    , m_profile(profile)
    , m_roots(roots)
    , m_bridge(new Bridge(this))
    , m_watcher(new FileWatcher(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    connect(m_bridge, &Bridge::pageReady, this, [this] {
        m_pageReady = true;
        pushState();
        pushDocument();
    });
    connect(m_bridge, &Bridge::dirtyChanged, this, &DocumentTab::setDirty);
    connect(m_bridge, &Bridge::activeHeadingReported, this, &DocumentTab::activeHeadingChanged);
    connect(m_bridge, &Bridge::statsReported, this, [this](int words, int characters) {
        m_words = words;
        emit statsChanged(words, characters);
    });
    connect(m_bridge, &Bridge::rendered, this, [this](int ms) {
        m_renderedPath = m_path;
        if (!m_pendingAnchor.isEmpty()) {
            emit m_bridge->scrollToAnchor(m_pendingAnchor);
            m_pendingAnchor.clear();
        }
        if (m_pendingLine > 0) {
            emit m_bridge->scrollToLine(m_pendingLine);
            m_pendingLine = 0;
        }
        emit rendered(ms);
    });
    connect(m_bridge, &Bridge::linkRequested, this, [this](const QString &href, bool newTab) {
        emit openUrlRequested(QUrl(href), newTab);
    });
    connect(m_bridge, &Bridge::headingsReported, this, [this](const QVariantList &headings) {
        m_headings = headings;
        emit headingsChanged(headings);
    });
    connect(m_watcher, &FileWatcher::changed, this, &DocumentTab::reload);
    connect(m_watcher, &FileWatcher::removed, this, &DocumentTab::fileRemoved);
}

DocumentTab::~DocumentTab()
{
    if (!m_rootKey.isEmpty())
        m_roots->remove(m_rootKey);
    // The page must be destroyed before the profile; make the order explicit.
    delete m_view;
}

void DocumentTab::ensurePage()
{
    if (m_view)
        return;
    m_view = new QWebEngineView(this);
    m_page = new WebPage(m_profile, appUrl(), m_view);
    m_channel = new QWebChannel(this);

    QWebEngineSettings *settings = m_page->settings();
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);

    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
    m_page->setWebChannel(m_channel);
    m_view->setPage(m_page);
    layout()->addWidget(m_view);

    connect(m_page, &WebPage::linkActivated, this, [this](const QUrl &url) {
        emit openUrlRequested(url, QApplication::keyboardModifiers() & Qt::ControlModifier);
    });
    connect(m_page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus status) {
        if (status == QWebEnginePage::NormalTerminationStatus)
            return;
        // Reload the frontend; pageReady re-sends the document. Unsaved edits
        // in the crashed renderer are lost, so say so.
        m_pageReady = false;
        if (m_dirty) {
            setDirty(false);
            qWarning("omnimark: renderer crashed with unsaved edits in %s", qPrintable(m_path));
        }
        m_page->load(appUrl());
    });
    m_view->installEventFilter(this);
    for (QWidget *child : m_view->findChildren<QWidget *>())
        child->installEventFilter(this);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested, this, &DocumentTab::showContextMenu);

    m_page->load(appUrl());
}

static bool hasLocalFiles(const QMimeData *mime)
{
    if (!mime || !mime->hasUrls())
        return false;
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile())
            return true;
    }
    return false;
}

bool DocumentTab::isImageFile(const QString &path)
{
    static const QStringList suffixes = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("gif"),
        QStringLiteral("webp"), QStringLiteral("svg"), QStringLiteral("bmp"), QStringLiteral("avif"),
    };
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

void DocumentTab::insertImageLinks(const QStringList &paths)
{
    // Images inside the document's folder are linked in place; others are
    // copied into ./images so the document stays self-contained.
    const QDir docDir(QFileInfo(m_path).absolutePath());
    QStringList lines;
    for (const QString &path : paths) {
        QString relative = docDir.relativeFilePath(path);
        if (relative.startsWith(QStringLiteral(".."))) {
            docDir.mkpath(QStringLiteral("images"));
            QString target = QStringLiteral("images/") + QFileInfo(path).fileName();
            if (QFileInfo::exists(docDir.filePath(target)))
                target = QStringLiteral("images/%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(QFileInfo(path).fileName());
            if (!QFile::copy(path, docDir.filePath(target)))
                continue;
            relative = target;
        }
        const QString alt = QFileInfo(path).completeBaseName();
        lines << QStringLiteral("![%1](%2)").arg(alt, QString::fromUtf8(QUrl::toPercentEncoding(relative, "/")));
    }
    if (lines.isEmpty())
        return;
    const QString json = QString::fromUtf8(QJsonDocument(QJsonArray { lines.join(u'\n') }).toJson(QJsonDocument::Compact));
    evaluate(QStringLiteral("window.omnimark.insertText(%1[0])").arg(json), [](const QVariant &) {});
}

bool DocumentTab::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::ChildAdded:
        // The web view creates its render widget lazily; filter it too.
        if (watched == m_view) {
            if (auto *child = qobject_cast<QWidget *>(static_cast<QChildEvent *>(event)->child()))
                child->installEventFilter(this);
        }
        break;
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        auto *drag = static_cast<QDragMoveEvent *>(event);
        if (hasLocalFiles(drag->mimeData())) {
            drag->acceptProposedAction();
            return true;
        }
        break;
    }
    case QEvent::Drop: {
        auto *drop = static_cast<QDropEvent *>(event);
        if (hasLocalFiles(drop->mimeData())) {
            drop->acceptProposedAction();
            QStringList images;
            for (const QUrl &url : drop->mimeData()->urls()) {
                if (!url.isLocalFile())
                    continue;
                const QString path = url.toLocalFile();
                if (m_viewMode != ViewMode::Preview && !m_path.isEmpty() && isImageFile(path))
                    images << path;
                else
                    emit localFileDropped(path);
            }
            if (!images.isEmpty())
                insertImageLinks(images);
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void DocumentTab::showContextMenu(const QPoint &pos)
{
    // Chromium's default menu offers Back/Reload/View Source, which make no
    // sense for an app page; offer editing and link/image actions only.
    QWebEngineContextMenuRequest *request = m_view->lastContextMenuRequest();
    QMenu menu(this);
    auto add = [&](QWebEnginePage::WebAction action) {
        QAction *a = m_page->action(action);
        if (a->isEnabled())
            menu.addAction(a);
    };
    if (request && request->isContentEditable()) {
        add(QWebEnginePage::Undo);
        add(QWebEnginePage::Redo);
        menu.addSeparator();
        add(QWebEnginePage::Cut);
    }
    add(QWebEnginePage::Copy);
    if (request && request->isContentEditable())
        add(QWebEnginePage::Paste);
    add(QWebEnginePage::SelectAll);
    if (request && request->linkUrl().isValid()) {
        menu.addSeparator();
        const QUrl link = request->linkUrl();
        menu.addAction(tr("Open Link"), this, [this, link] { emit openUrlRequested(link, false); });
        if (link.scheme() == u"omnimark")
            menu.addAction(tr("Open Link in New Tab"), this, [this, link] { emit openUrlRequested(link, true); });
        add(QWebEnginePage::CopyLinkToClipboard);
    }
    if (request && request->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage)
        add(QWebEnginePage::CopyImageToClipboard);
    if (!qEnvironmentVariableIsEmpty("QTWEBENGINE_REMOTE_DEBUGGING")) {
        menu.addSeparator();
        add(QWebEnginePage::InspectElement);
    }
    if (!menu.isEmpty())
        menu.exec(m_view->mapToGlobal(pos));
}

void DocumentTab::showEvent(QShowEvent *event)
{
    ensurePage();
    QWidget::showEvent(event);
}

QString DocumentTab::title() const
{
    return m_path.isEmpty() ? tr("Untitled") : QFileInfo(m_path).fileName();
}

void DocumentTab::loadUntitled(const QString &text)
{
    m_path.clear();
    m_text = text;
    m_watcher->setPath({});
    emit pathChanged(m_path);
    pushDocument();
}

void DocumentTab::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged(dirty);
}

void DocumentTab::evaluate(const QString &script, const std::function<void(const QVariant &)> &callback)
{
    if (!m_page || !m_pageReady) {
        callback(QVariant());
        return;
    }
    m_page->runJavaScript(script, callback);
}

void DocumentTab::save(const SaveCallback &done)
{
    if (m_path.isEmpty()) {
        done(false, tr("Document has no file name"));
        return;
    }
    writeFile(m_path, done);
}

void DocumentTab::saveAs(const QString &path, const SaveCallback &done)
{
    writeFile(QFileInfo(path).absoluteFilePath(), done);
}

void DocumentTab::writeFile(const QString &path, const SaveCallback &done)
{
    if (!m_pageReady) {
        done(false, tr("Document is still loading"));
        return;
    }
    QPointer<DocumentTab> self(this);
    m_page->runJavaScript(QStringLiteral("window.omnimark.snapshot()"), [self, path, done](const QVariant &result) {
        if (!self) {
            done(false, QStringLiteral("Tab was closed"));
            return;
        }
        // [id, text]. Anything else means the page is gone or broken; never
        // write an empty file over the user's document in that case.
        const QVariantList snapshot = result.toList();
        if (snapshot.size() != 2 || snapshot[1].typeId() != QMetaType::QString) {
            done(false, tr("Could not read the editor contents"));
            return;
        }
        const int id = snapshot[0].toInt();
        const QString text = snapshot[1].toString();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            done(false, file.errorString());
            return;
        }
        file.write(text.toUtf8());
        if (!file.commit()) {
            done(false, file.errorString());
            return;
        }
        const bool samePath = path == self->m_path;
        if (samePath)
            self->m_watcher->ignoreNextChange();
        self->m_text = text;
        self->m_conflictPending = false;
        self->m_page->runJavaScript(QStringLiteral("window.omnimark.markSaved(%1)").arg(id));
        if (!samePath) {
            // Save As: adopt the new location without disturbing the editor.
            const QFileInfo info(path);
            if (!self->m_rootKey.isEmpty())
                self->m_roots->remove(self->m_rootKey);
            self->m_path = info.canonicalFilePath();
            self->m_rootKey = self->m_roots->add(info.absolutePath());
            self->m_bridge->setDocumentDirectory(info.absolutePath());
            self->m_watcher->setPath(self->m_path);
            emit self->pathChanged(self->m_path);
            self->pushDocument();
        }
        done(true, QString());
    });
}

void DocumentTab::revert()
{
    QString text;
    if (!m_path.isEmpty() && readFile(m_path, &text, nullptr))
        m_text = text;
    m_conflictPending = false;
    pushDocument();
}

void DocumentTab::keepLocalVersion()
{
    QString text;
    if (!m_path.isEmpty() && readFile(m_path, &text, nullptr))
        m_text = text;
    m_conflictPending = false;
}

void DocumentTab::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode)
        return;
    m_viewMode = mode;
    pushState();
    emit viewModeChanged(mode);
}

void DocumentTab::setTheme(const QString &theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    if (m_pageReady)
        emit m_bridge->themeChanged(theme);
}

void DocumentTab::setSettingsJson(const QString &json)
{
    if (m_settingsJson == json)
        return;
    m_settingsJson = json;
    if (m_pageReady)
        emit m_bridge->settingsChanged(json);
}

void DocumentTab::scrollToLine(double line)
{
    if (m_pageReady && m_renderedPath == m_path)
        emit m_bridge->scrollToLine(line);
    else
        m_pendingLine = line;
}

void DocumentTab::pushState()
{
    if (!m_pageReady)
        return;
    static const char *modes[] = { "preview", "split", "source" };
    if (!m_theme.isEmpty())
        emit m_bridge->themeChanged(m_theme);
    if (!m_settingsJson.isEmpty())
        emit m_bridge->settingsChanged(m_settingsJson);
    emit m_bridge->viewModeChanged(QString::fromLatin1(modes[int(m_viewMode)]));
}

bool DocumentTab::readFile(const QString &path, QString *text, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    *text = QString::fromUtf8(file.readAll());
    return true;
}

void DocumentTab::setDocument(const QString &path, const QString &text)
{
    const QFileInfo info(path);
    const QString previousKey = m_rootKey;
    m_path = info.canonicalFilePath();
    m_text = text;
    m_renderedPath.clear();
    m_headings.clear();
    m_rootKey = m_roots->add(info.absolutePath());
    if (!previousKey.isEmpty())
        m_roots->remove(previousKey);
    m_bridge->setDocumentDirectory(info.absolutePath());
    m_watcher->setPath(m_path);
    emit pathChanged(m_path);
    pushDocument();
}

bool DocumentTab::load(const QString &path, QString *error)
{
    QString text;
    if (!readFile(QFileInfo(path).absoluteFilePath(), &text, error))
        return false;
    setDocument(path, text);
    return true;
}

bool DocumentTab::navigate(const QString &path, const QString &fragment, QString *error)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (canonical == m_path) {
        if (!fragment.isEmpty())
            scrollToAnchor(fragment);
        return true;
    }
    QString text;
    if (!readFile(path, &text, error))
        return false;
    if (!m_path.isEmpty())
        m_back.append(m_path);
    m_forward.clear();
    m_pendingAnchor = fragment;
    setDocument(path, text);
    emit historyChanged();
    return true;
}

void DocumentTab::goBack()
{
    if (m_back.isEmpty())
        return;
    const QString target = m_back.takeLast();
    QString text;
    if (!readFile(target, &text, nullptr)) {
        emit historyChanged();
        return;
    }
    m_forward.append(m_path);
    setDocument(target, text);
    emit historyChanged();
}

void DocumentTab::goForward()
{
    if (m_forward.isEmpty())
        return;
    const QString target = m_forward.takeLast();
    QString text;
    if (!readFile(target, &text, nullptr)) {
        emit historyChanged();
        return;
    }
    m_back.append(m_path);
    setDocument(target, text);
    emit historyChanged();
}

void DocumentTab::scrollToAnchor(const QString &id)
{
    if (m_pageReady && m_renderedPath == m_path)
        emit m_bridge->scrollToAnchor(id);
    else
        m_pendingAnchor = id;
}

void DocumentTab::reload()
{
    QString text;
    if (!readFile(m_path, &text, nullptr) || text == m_text)
        return;
    if (m_dirty) {
        // Don't clobber unsaved edits; let the window ask what to do.
        if (!m_conflictPending) {
            m_conflictPending = true;
            emit externalChangeConflict();
        }
        return;
    }
    m_text = text;
    pushDocument();
}

void DocumentTab::pushDocument()
{
    if (!m_pageReady)
        return;
    if (m_path.isEmpty()) {
        emit m_bridge->documentLoaded(QString(), m_text, QString());
        return;
    }
    const QString dir = QFileInfo(m_path).absolutePath();
    QString base = QStringLiteral("omnimark://local") + QUrl::toPercentEncoding(QDir::fromNativeSeparators(dir), "/:");
    if (!base.startsWith(QStringLiteral("omnimark://local/")))
        base.insert(16, u'/'); // Windows drive paths: omnimark://local/C:/...
    if (!base.endsWith(u'/'))
        base += u'/';
    emit m_bridge->documentLoaded(m_path, m_text, base);
}

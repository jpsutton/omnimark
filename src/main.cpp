#include "Bridge.h"
#include "DocumentTab.h"
#include "Icons.h"
#include "Exporter.h"
#include "LocalRoots.h"
#include "MainWindow.h"
#include "SchemeHandler.h"
#include "Settings.h"
#include "SingleInstance.h"
#include "WebAssets.h"
#include "WebPage.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFileOpenEvent>
#include <QIcon>
#include <QFileInfo>
#include <QPageLayout>
#include <QStandardPaths>
#include <QPageSize>
#include <QTimer>
#include <QWebEngineProfile>
#include <QWebEngineView>

#include <cstdio>
#include <functional>

// Headless-ish smoke mode: open a file, wait for the first render, export a
// PDF, then exit. Used by CI and for quick manual verification.
static void runRenderToPdf(MainWindow &window, Exporter &exporter, const QString &input, const QString &output,
                           int timeoutMs)
{
    DocumentTab *tab = window.openFile(input);
    if (!tab) {
        QCoreApplication::exit(2);
        return;
    }
    static QElapsedTimer clock;
    clock.start();
    QObject::connect(tab, &DocumentTab::rendered, tab, [tab, &exporter, output](int ms) {
        std::fprintf(stderr, "omnimark: rendered in %d ms (%lld ms since start)\n", ms, clock.elapsed());
        exporter.exportPdf(tab, output, QPageSize(QPageSize::A4), QMarginsF(15, 15, 15, 15),
                           [output](bool ok, const QString &error) {
            std::fprintf(stderr, "omnimark: pdf %s -> %s %s (%lld ms since start)\n", ok ? "ok" : "FAILED",
                         qPrintable(output), qPrintable(error), clock.elapsed());
            QCoreApplication::exit(ok ? 0 : 3);
        });
    }, Qt::SingleShotConnection);
    QTimer::singleShot(timeoutMs, qApp, [] {
        std::fprintf(stderr, "omnimark: timed out after %lld ms\n", clock.elapsed());
        QCoreApplication::exit(4);
    });
}

// macOS delivers Finder "Open With" and dock drops as QFileOpenEvent.
class FileOpenFilter : public QObject
{
public:
    std::function<void(const QString &)> handler;
    QStringList pending;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen) {
            const QString file = static_cast<QFileOpenEvent *>(event)->file();
            if (handler)
                handler(file);
            else
                pending << file;
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

// Debug aid: grab the whole window once the current tab has rendered.
static void scheduleScreenshot(MainWindow &window, const QString &output, int delayMs)
{
    auto take = [&window, output, delayMs] {
        QTimer::singleShot(delayMs, &window, [&window, output] {
            const bool ok = window.grab().save(output);
            std::fprintf(stderr, "omnimark: screenshot %s -> %s\n", ok ? "ok" : "FAILED", qPrintable(output));
            QCoreApplication::exit(ok ? 0 : 3);
        });
    };
    if (DocumentTab *tab = window.currentTab())
        QObject::connect(tab, &DocumentTab::rendered, &window, take, Qt::SingleShotConnection);
    else
        take();
}

int main(int argc, char *argv[])
{
    SchemeHandler::registerScheme();

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Omnimark"));
    QApplication::setApplicationName(QStringLiteral("Omnimark"));
    QApplication::setApplicationVersion(QStringLiteral(OMNIMARK_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(OMNIMARK_APP_ID));
    Icons::ensureTheme();
    {
        QIcon icon;
        for (int size : { 16, 32, 48, 64, 128, 256, 512 })
            icon.addFile(QStringLiteral(":/packaging/icons/omnimark-%1.png").arg(size), QSize(size, size));
        QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(OMNIMARK_APP_ID), icon));
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Markdown viewer and editor"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Markdown files to open."),
                                 QStringLiteral("[files...]"));
    QCommandLineOption pdfOption(QStringLiteral("render-pdf"),
                                 QStringLiteral("Render the first file to <pdf> and exit."),
                                 QStringLiteral("pdf"));
    QCommandLineOption timeoutOption(QStringLiteral("timeout"),
                                     QStringLiteral("Timeout for --render-pdf in ms."),
                                     QStringLiteral("ms"), QStringLiteral("30000"));
    QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                        QStringLiteral("Save a window screenshot to <png> after rendering, then exit."),
                                        QStringLiteral("png"));
    QCommandLineOption viewOption(QStringLiteral("view"),
                                  QStringLiteral("Initial view for opened files: preview, split or source."),
                                  QStringLiteral("mode"));
    QCommandLineOption newInstanceOption(QStringLiteral("new-instance"),
                                         QStringLiteral("Don't hand files to an already running window."));
    parser.addOption(pdfOption);
    parser.addOption(timeoutOption);
    parser.addOption(newInstanceOption);
    parser.addOption(screenshotOption);
    parser.addOption(viewOption);
    parser.process(app);

    FileOpenFilter fileOpenFilter;
    app.installEventFilter(&fileOpenFilter);

    QStringList files;
    for (const QString &file : parser.positionalArguments())
        files << QFileInfo(file).absoluteFilePath();

    const bool standalone = parser.isSet(pdfOption) || parser.isSet(newInstanceOption)
        || parser.isSet(screenshotOption);
    // Smoke/debug modes must not touch the user's recent files or session.
    if (parser.isSet(pdfOption) || parser.isSet(screenshotOption))
        QStandardPaths::setTestModeEnabled(true);
    SingleInstance instance(SingleInstance::defaultKey());
    if (!standalone) {
        if (instance.forwardToPrimary(files))
            return 0;
        instance.listen();
    }

    const QString webDir = WebAssets::directory();
    if (webDir.isEmpty() && WebAssets::devServerUrl().isEmpty()) {
        std::fprintf(stderr, "omnimark: web frontend not found; build web/ or set OMNIMARK_WEB_DIR\n");
        return 1;
    }

    LocalRoots roots;
    QWebEngineProfile profile(QStringLiteral("omnimark"));
    SchemeHandler handler(webDir, &roots);
    profile.installUrlSchemeHandler(SchemeHandler::Scheme, &handler);

    int exitCode = 0;
    {
        Settings settings;
        MainWindow window(&profile, &roots, &settings);
        auto openForwarded = [&window](const QStringList &paths) {
            for (const QString &path : paths) {
                if (QFileInfo(path).isDir())
                    window.openFolder(path);
                else
                    window.openFile(path);
            }
            window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
            window.raise();
            window.activateWindow();
        };
        QObject::connect(&instance, &SingleInstance::filesReceived, &window, openForwarded);
        fileOpenFilter.handler = [&window](const QString &path) { window.openFile(path); };

        if (parser.isSet(pdfOption)) {
            if (files.isEmpty()) {
                std::fprintf(stderr, "omnimark: --render-pdf needs an input file\n");
                return 1;
            }
            window.show();
            static Exporter exporter;
            runRenderToPdf(window, exporter, files.first(), QFileInfo(parser.value(pdfOption)).absoluteFilePath(),
                           parser.value(timeoutOption).toInt());
        } else {
            const QStringList initial = files + fileOpenFilter.pending;
            if (initial.isEmpty())
                window.restoreSession();
            static const QStringList viewModes = {
                QStringLiteral("preview"), QStringLiteral("split"), QStringLiteral("source"),
            };
            const int viewMode = viewModes.indexOf(parser.value(viewOption));
            for (const QString &file : initial) {
                if (QFileInfo(file).isDir()) {
                    window.openFolder(file);
                } else if (DocumentTab *tab = window.openFile(file); tab && viewMode >= 0) {
                    tab->setViewMode(DocumentTab::ViewMode(viewMode));
                }
            }
            window.show();
            if (parser.isSet(screenshotOption))
                scheduleScreenshot(window, QFileInfo(parser.value(screenshotOption)).absoluteFilePath(), 800);
        }
        exitCode = app.exec();
        fileOpenFilter.handler = nullptr;
    }
    return exitCode;
}

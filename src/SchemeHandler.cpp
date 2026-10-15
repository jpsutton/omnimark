#include "SchemeHandler.h"
#include "LocalRoots.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

void SchemeHandler::registerScheme()
{
    QWebEngineUrlScheme scheme(Scheme);
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme
                    | QWebEngineUrlScheme::LocalAccessAllowed
                    | QWebEngineUrlScheme::CorsEnabled
                    | QWebEngineUrlScheme::FetchApiAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
}

SchemeHandler::SchemeHandler(const QString &webDir, const LocalRoots *roots, QObject *parent)
    : QWebEngineUrlSchemeHandler(parent)
    , m_webDir(QDir::cleanPath(webDir))
    , m_roots(roots)
{
}

QByteArray SchemeHandler::mimeTypeFor(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == u"js" || suffix == u"mjs")
        return "text/javascript";
    if (suffix == u"wasm")
        return "application/wasm";
    if (suffix == u"css")
        return "text/css";
    if (suffix == u"html" || suffix == u"htm")
        return "text/html";
    if (suffix == u"svg")
        return "image/svg+xml";
    if (suffix == u"json" || suffix == u"map")
        return "application/json";
    if (suffix == u"woff2")
        return "font/woff2";
    if (suffix == u"woff")
        return "font/woff";
    if (suffix == u"ttf")
        return "font/ttf";
    static const QMimeDatabase db;
    return db.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name().toUtf8();
}

QString SchemeHandler::resolveAppPath(const QString &urlPath) const
{
    if (m_webDir.isEmpty())
        return {};
    QString relative = QDir::cleanPath(urlPath);
    if (relative.isEmpty() || relative == u"/")
        relative = QStringLiteral("/index.html");
    if (relative.startsWith(u"/.."))
        return {};
    const QString candidate = QDir::cleanPath(m_webDir + relative);
    if (!candidate.startsWith(m_webDir + u'/'))
        return {};
    const QFileInfo info(candidate);
    if (!info.isFile())
        return {};
    return candidate;
}

QString SchemeHandler::resolveLocalPath(const QString &urlPath) const
{
    QString path = urlPath;
#ifdef Q_OS_WIN
    // omnimark://local/C:/dir/file.png -> "/C:/dir/file.png"
    if (path.size() > 2 && path.at(0) == u'/' && path.at(2) == u':')
        path.remove(0, 1);
#endif
    if (!m_roots || !m_roots->isAllowed(path))
        return {};
    return QFileInfo(path).canonicalFilePath();
}

void SchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    const QUrl url = job->requestUrl();
    const QString host = url.host();
    const QString urlPath = url.path(QUrl::FullyDecoded);

    if (job->requestMethod() != "GET") {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }

    QString file;
    if (host == u"app") {
        if (urlPath == u"/qwebchannel.js")
            file = QStringLiteral(":/qtwebchannel/qwebchannel.js");
        else
            file = resolveAppPath(urlPath);
    } else if (host == u"local") {
        file = resolveLocalPath(urlPath);
        if (file.isEmpty()) {
            // Don't reveal whether files outside the allowed roots exist.
            const bool missing = m_roots && !QFileInfo::exists(urlPath)
                && m_roots->containsDirectory(QFileInfo(urlPath).absolutePath());
            job->fail(missing ? QWebEngineUrlRequestJob::UrlNotFound
                              : QWebEngineUrlRequestJob::RequestDenied);
            return;
        }
    }

    if (file.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    auto *device = new QFile(file, job);
    if (!device->open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    job->reply(mimeTypeFor(file), device);
}

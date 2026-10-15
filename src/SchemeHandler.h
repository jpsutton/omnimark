#pragma once

#include <QWebEngineUrlSchemeHandler>

class LocalRoots;

// Serves the omnimark:// scheme:
//   omnimark://app/<path>    -> built web frontend (plus qwebchannel.js from Qt resources)
//   omnimark://local/<path>  -> local files inside LocalRoots (images, linked assets)
class SchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    static constexpr const char *Scheme = "omnimark";

    // Must run before QApplication is constructed.
    static void registerScheme();

    SchemeHandler(const QString &webDir, const LocalRoots *roots, QObject *parent = nullptr);

    void requestStarted(QWebEngineUrlRequestJob *job) override;

    // Exposed for tests: map a URL to a file path, or empty if not servable.
    QString resolveAppPath(const QString &urlPath) const;
    QString resolveLocalPath(const QString &urlPath) const;

    static QByteArray mimeTypeFor(const QString &path);

private:
    QString m_webDir;
    const LocalRoots *m_roots;
};

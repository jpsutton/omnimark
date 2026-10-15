#pragma once

#include <QObject>
#include <QVariantList>

// QObject published to the page as `bridge` through QWebChannel.
// Methods are called from JS; signals are delivered to JS listeners.
class Bridge : public QObject
{
    Q_OBJECT
public:
    explicit Bridge(QObject *parent = nullptr);

    // Directory of the current document; empty for untitled documents.
    void setDocumentDirectory(const QString &dir) { m_documentDirectory = dir; }
    static QString imageFileName(const QString &mimeType);

    // --- called from JS ---
    Q_INVOKABLE void ready();
    Q_INVOKABLE void renderFinished(int elapsedMs);
    Q_INVOKABLE void openLink(const QString &href, bool newTab);
    Q_INVOKABLE void reportHeadings(const QVariantList &headings);
    Q_INVOKABLE void reportActiveHeading(const QString &id);
    Q_INVOKABLE void setDirty(bool dirty);
    Q_INVOKABLE void exportFinished(const QString &html, const QString &error);
    Q_INVOKABLE void reportStats(int words, int characters);
    // Saves pasted image bytes (base64) next to the document; returns the
    // Markdown-ready relative path, or an empty string on failure.
    Q_INVOKABLE QString saveImage(const QString &base64, const QString &mimeType);
    Q_INVOKABLE void log(const QString &message);

signals:
    // --- to JS ---
    void documentLoaded(const QString &path, const QString &text, const QString &baseUrl);
    void themeChanged(const QString &theme);
    void viewModeChanged(const QString &mode);
    void settingsChanged(const QString &json);
    void scrollToAnchor(const QString &id);
    void scrollToLine(double line);

    // --- to C++ ---
    void pageReady();
    void rendered(int elapsedMs);
    void linkRequested(const QString &href, bool newTab);
    void headingsReported(const QVariantList &headings);
    void activeHeadingReported(const QString &id);
    void dirtyChanged(bool dirty);
    void htmlExported(const QString &html, const QString &error);
    void statsReported(int words, int characters);
    void imageSaved(const QString &path);

private:
    QString m_documentDirectory;
};

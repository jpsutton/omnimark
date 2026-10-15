#pragma once

#include <QWebEnginePage>

// Keeps the page pinned to the app frontend: link clicks are handed to C++
// (linkActivated) and every other navigation away is refused.
class WebPage : public QWebEnginePage
{
    Q_OBJECT
public:
    WebPage(QWebEngineProfile *profile, const QUrl &appUrl, QObject *parent = nullptr);

signals:
    void linkActivated(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int line, const QString &source) override;

private:
    QUrl m_appUrl;
};

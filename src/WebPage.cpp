#include "WebPage.h"

#include <QDebug>

WebPage::WebPage(QWebEngineProfile *profile, const QUrl &appUrl, QObject *parent)
    : QWebEnginePage(profile, parent)
    , m_appUrl(appUrl)
{
}

bool WebPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    if (!isMainFrame)
        return url.scheme() == u"omnimark" || url.scheme() == u"about" || url.scheme() == u"data";

    const bool sameApp = url.scheme() == m_appUrl.scheme() && url.host() == m_appUrl.host()
        && url.port() == m_appUrl.port();
    if (sameApp && type != NavigationTypeLinkClicked)
        return true;

    // file:// navigations (e.g. a script, or a drop that slipped past
    // DocumentTab's event filter) are always refused.
    if (type == NavigationTypeLinkClicked && !url.isLocalFile())
        emit linkActivated(url);
    return false;
}

void WebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                       int line, const QString &source)
{
    const QString where = source.section(u'/', -1) + u':' + QString::number(line);
    switch (level) {
    case ErrorMessageLevel:
        qWarning().noquote() << "[js error]" << where << message;
        break;
    case WarningMessageLevel:
        qWarning().noquote() << "[js warn]" << where << message;
        break;
    default:
        qDebug().noquote() << "[js]" << where << message;
        break;
    }
}

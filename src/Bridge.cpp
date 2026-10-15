#include "Bridge.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

Bridge::Bridge(QObject *parent)
    : QObject(parent)
{
}

void Bridge::ready()
{
    emit pageReady();
}

void Bridge::renderFinished(int elapsedMs)
{
    emit rendered(elapsedMs);
}

void Bridge::openLink(const QString &href, bool newTab)
{
    emit linkRequested(href, newTab);
}

void Bridge::reportHeadings(const QVariantList &headings)
{
    emit headingsReported(headings);
}

void Bridge::reportActiveHeading(const QString &id)
{
    emit activeHeadingReported(id);
}

void Bridge::setDirty(bool dirty)
{
    emit dirtyChanged(dirty);
}

void Bridge::exportFinished(const QString &html, const QString &error)
{
    emit htmlExported(html, error);
}

void Bridge::reportStats(int words, int characters)
{
    emit statsReported(words, characters);
}

QString Bridge::imageFileName(const QString &mimeType)
{
    QString suffix = QStringLiteral("png");
    if (mimeType == u"image/jpeg")
        suffix = QStringLiteral("jpg");
    else if (mimeType == u"image/gif")
        suffix = QStringLiteral("gif");
    else if (mimeType == u"image/webp")
        suffix = QStringLiteral("webp");
    else if (mimeType == u"image/svg+xml")
        suffix = QStringLiteral("svg");
    return QStringLiteral("pasted-%1.%2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")), suffix);
}

QString Bridge::saveImage(const QString &base64, const QString &mimeType)
{
    if (m_documentDirectory.isEmpty() || !mimeType.startsWith(u"image/"))
        return {};
    const QByteArray data = QByteArray::fromBase64(base64.toLatin1());
    if (data.isEmpty() || data.size() > 50 * 1024 * 1024)
        return {};
    const QDir dir(m_documentDirectory);
    if (!dir.mkpath(QStringLiteral("images")))
        return {};
    const QString relative = QStringLiteral("images/") + imageFileName(mimeType);
    QFile file(dir.filePath(relative));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(data) != data.size())
        return {};
    emit imageSaved(file.fileName());
    return relative;
}

void Bridge::log(const QString &message)
{
    qInfo().noquote() << "[web]" << message;
}

#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
}

QString SingleInstance::defaultKey()
{
    const QByteArray user = qgetenv("USER").isEmpty() ? qgetenv("USERNAME") : qgetenv("USER");
    const QByteArray hash = QCryptographicHash::hash(user + QDir::homePath().toUtf8(),
                                                     QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("omnimark-") + QString::fromLatin1(hash);
}

QByteArray SingleInstance::encode(const QStringList &files)
{
    return QJsonDocument(QJsonArray::fromStringList(files)).toJson(QJsonDocument::Compact) + '\n';
}

QStringList SingleInstance::decode(const QByteArray &data)
{
    QStringList files;
    const QJsonDocument doc = QJsonDocument::fromJson(data.trimmed());
    for (const QJsonValue &value : doc.array()) {
        if (value.isString())
            files << value.toString();
    }
    return files;
}

bool SingleInstance::forwardToPrimary(const QStringList &files, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (!socket.waitForConnected(timeoutMs))
        return false;
    socket.write(encode(files));
    if (!socket.waitForBytesWritten(timeoutMs))
        return false;
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(timeoutMs);
    return true;
}

bool SingleInstance::listen()
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_key)) {
        // A crashed instance can leave a stale socket file behind.
        QLocalServer::removeServer(m_key);
        if (!m_server->listen(m_key))
            return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QLocalSocket::readyRead, socket, [socket, buffer] {
                buffer->append(socket->readAll());
            });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket, buffer] {
                buffer->append(socket->readAll());
                socket->deleteLater();
                emit filesReceived(decode(*buffer));
            });
        }
    });
    return true;
}

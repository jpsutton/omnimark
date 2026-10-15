#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

// Ensures one running Omnimark per user session. A second launch forwards its
// file arguments to the primary instance over a local socket and exits.
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);

    // Tries to hand `files` to a running instance. Returns true on success,
    // meaning this process should exit.
    bool forwardToPrimary(const QStringList &files, int timeoutMs = 1000);

    // Starts listening for forwarded launches. Returns false on failure.
    bool listen();

    static QString defaultKey();
    static QByteArray encode(const QStringList &files);
    static QStringList decode(const QByteArray &data);

signals:
    void filesReceived(const QStringList &files);

private:
    QString m_key;
    QLocalServer *m_server = nullptr;
};

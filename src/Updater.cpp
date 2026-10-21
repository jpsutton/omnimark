#include "Updater.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

namespace {

const QString ManifestUrl = QStringLiteral("https://github.com/jpsutton/omnimark/releases/latest/download/update.json");

QString bundlePath()
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../.."));
}

QString scriptPath()
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/install-macos.sh"));
}

QString lastLine(const QByteArray &output)
{
    const QStringList lines = QString::fromUtf8(output).trimmed().split(u'\n');
    return lines.isEmpty() ? QString() : lines.last().trimmed();
}

} // namespace

Updater::Updater(QObject *parent)
    : QObject(parent)
{
}

Updater::~Updater()
{
    cancel();
}

bool Updater::isSupported()
{
#ifdef Q_OS_MACOS
    return !QStringLiteral(OMNIMARK_BUILD_COMMIT).isEmpty() && QFileInfo::exists(scriptPath());
#else
    return false;
#endif
}

std::optional<Updater::Release> Updater::parseManifest(const QByteArray &json, const QString &ownCommit)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    Release release { object.value(QStringLiteral("version")).toString(),
                      object.value(QStringLiteral("commit")).toString() };
    static const QRegularExpression sha(QStringLiteral("^[0-9a-f]{40}$"));
    if (release.version.isEmpty() || !sha.match(release.commit).hasMatch() || release.commit == ownCommit)
        return std::nullopt;
    return release;
}

QString Updater::installDir()
{
    return QFileInfo(bundlePath()).absolutePath();
}

QProcess *Updater::startScript(const QString &program, const QStringList &args)
{
    m_process = new QProcess(this);
    m_process->start(program, args);
    return m_process;
}

void Updater::check()
{
    if (m_process)
        return;
    QProcess *process = startScript(QStringLiteral("/usr/bin/curl"),
                                    { QStringLiteral("-fsSL"), QStringLiteral("--max-time"), QStringLiteral("30"),
                                      ManifestUrl });
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        m_process = nullptr;
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            emit checkFailed(lastLine(process->readAllStandardError()));
            return;
        }
        if (const auto release = parseManifest(process->readAllStandardOutput(), QStringLiteral(OMNIMARK_BUILD_COMMIT)))
            emit updateAvailable(*release);
        else
            emit upToDate();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_process = nullptr;
        process->deleteLater();
        emit checkFailed(process->errorString());
    });
}

void Updater::stage()
{
    if (m_process)
        return;
    m_staged.clear();
    QProcess *process = startScript(QStringLiteral("/bin/bash"), { scriptPath(), QStringLiteral("--stage") });
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        m_process = nullptr;
        process->deleteLater();
        const QString path = lastLine(process->readAllStandardOutput());
        if (status != QProcess::NormalExit || code != 0 || !path.endsWith(QStringLiteral(".app"))
            || !QFileInfo(path).isDir()) {
            const QString error = lastLine(process->readAllStandardError());
            emit stageFailed(error.isEmpty() ? tr("The download failed.") : error);
            return;
        }
        m_staged = path;
        emit staged();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_process = nullptr;
        process->deleteLater();
        emit stageFailed(process->errorString());
    });
}

void Updater::cancel()
{
    if (!m_process)
        return;
    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    process->kill();
    process->waitForFinished(2000);
    process->deleteLater();
}

void Updater::applyOnExit(bool relaunch)
{
    if (m_staged.isEmpty())
        return;
    const QString logDir = QDir::homePath() + QStringLiteral("/Library/Logs/Omnimark");
    QDir().mkpath(logDir);
    QStringList args = { scriptPath(),
                         QStringLiteral("--apply"), m_staged,
                         QStringLiteral("--app"), bundlePath(),
                         QStringLiteral("--wait-pid"), QString::number(QCoreApplication::applicationPid()) };
    if (relaunch)
        args << QStringLiteral("--relaunch");
    QProcess process;
    process.setProgram(QStringLiteral("/bin/bash"));
    process.setArguments(args);
    process.setStandardOutputFile(logDir + QStringLiteral("/update.log"), QIODevice::Append);
    process.setStandardErrorFile(logDir + QStringLiteral("/update.log"), QIODevice::Append);
    process.startDetached();
    m_staged.clear();
}

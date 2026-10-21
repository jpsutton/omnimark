#pragma once

#include <QObject>
#include <QString>

#include <optional>

class QProcess;

// Self-update for the macOS app. Release builds carry the commit they were
// built from (OMNIMARK_BUILD_COMMIT); each release publishes update.json with
// its tag and commit. A different commit means a newer release, since only
// released builds carry a commit. The bundled install-macos.sh does the work:
// --stage downloads and verifies the new app while Omnimark runs, then
// --apply swaps the bundle once Omnimark has quit. curl downloads aren't
// quarantined, so the new version opens without a Gatekeeper prompt.
class Updater : public QObject
{
    Q_OBJECT
public:
    struct Release {
        QString version;
        QString commit;
    };

    explicit Updater(QObject *parent = nullptr);
    ~Updater() override;

    // True only for macOS release builds that bundle the install script.
    static bool isSupported();
    // The release in an update.json, if it differs from `ownCommit`.
    static std::optional<Release> parseManifest(const QByteArray &json, const QString &ownCommit);
    // Folder the app bundle lives in, for checking it's writable.
    static QString installDir();

    bool isBusy() const { return m_process != nullptr; }
    bool isStaged() const { return !m_staged.isEmpty(); }

    void check();
    void stage();
    void cancel();
    // Starts the script that swaps the bundle once this process exits.
    // Call when the app is quitting.
    void applyOnExit(bool relaunch);

signals:
    void updateAvailable(const Updater::Release &release);
    void upToDate();
    void checkFailed(const QString &error);
    void staged();
    void stageFailed(const QString &error);

private:
    QProcess *startScript(const QString &program, const QStringList &args);

    QProcess *m_process = nullptr;
    QString m_staged;
};

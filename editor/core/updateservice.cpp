#include "updateservice.h"

#include "documentmanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QVersionNumber>

#ifndef DME_GIT_COMMIT
#define DME_GIT_COMMIT "unknown"
#endif

#ifndef DME_BUILD_CHANNEL
#define DME_BUILD_CHANNEL "stable"
#endif

namespace {
constexpr auto kRepositoryApi = "https://api.github.com/repos/dewral/DewralMapEditor";
constexpr auto kWindowsArchive = "DewralMapEditor-windows-x64.zip";
constexpr auto kManifestName = "update-manifest.json";

QNetworkRequest githubRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "DewralMapEditor-Updater");
    request.setTransferTimeout(30000);
    return request;
}

QString normalizedVersion(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        value.remove(0, 1);
    return value;
}

QString digestValue(QString digest)
{
    if (digest.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive))
        digest.remove(0, 7);
    return digest.trimmed().toLower();
}

bool validSha256(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{64}$"));
    return pattern.match(value).hasMatch();
}

bool validPackageUrl(const QUrl &url)
{
    return url.scheme() == QStringLiteral("https")
        && url.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) == 0
        && url.path().endsWith(QStringLiteral("/") + QLatin1String(kWindowsArchive));
}
}

UpdateService::UpdateService(DocumentManager *documents, QObject *parent)
    : QObject(parent), m_documents(documents)
{
}

QString UpdateService::currentVersion() const
{
    return QCoreApplication::applicationVersion();
}

QString UpdateService::currentCommit() const
{
    return QStringLiteral(DME_GIT_COMMIT);
}

QString UpdateService::currentChannel() const
{
    return QStringLiteral(DME_BUILD_CHANNEL);
}

bool UpdateService::busy() const
{
    return m_state == QStringLiteral("checking")
        || m_state == QStringLiteral("downloading")
        || m_state == QStringLiteral("installing");
}

void UpdateService::resetRelease()
{
    m_latestVersion.clear();
    m_latestCommit.clear();
    m_releaseNotes.clear();
    m_releasePageUrl.clear();
    m_downloadUrl.clear();
    m_expectedSha256.clear();
    m_expectedSize = -1;
    m_downloadProgress = 0.0;
    m_updateAvailable = false;
    emit updateChanged();
    emit downloadProgressChanged();
}

void UpdateService::checkForUpdates(const QString &channel, bool silent)
{
    if (busy())
        return;

    const bool developmentBuild = currentChannel() == QStringLiteral("development");
    m_requestedChannel = developmentBuild || channel == QStringLiteral("development")
        ? QStringLiteral("development") : QStringLiteral("stable");
    m_silent = silent;
    m_cancelled = false;

    if (silent) {
        QSettings settings;
        const QString key = QStringLiteral("updates/lastCheckUtc/%1").arg(m_requestedChannel);
        const QDateTime last = settings.value(key).toDateTime();
        if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < 24 * 60 * 60)
            return;
    }

    resetRelease();
    setState(QStringLiteral("checking"));
    requestRelease(m_requestedChannel);
}

void UpdateService::requestRelease(const QString &channel)
{
    const QString endpoint = channel == QStringLiteral("development")
        ? QStringLiteral("%1/releases/tags/nightly").arg(QLatin1String(kRepositoryApi))
        : QStringLiteral("%1/releases/latest").arg(QLatin1String(kRepositoryApi));
    startRequest(QUrl(endpoint), [this](QByteArray payload) { processRelease(payload); });
}

void UpdateService::startRequest(const QUrl &url,
                                 const std::function<void(QByteArray)> &handler)
{
    m_reply = m_network.get(githubRequest(url));
    connect(m_reply, &QNetworkReply::finished, this, [this, handler]() {
        QNetworkReply *reply = m_reply;
        if (!reply)
            return;
        m_reply.clear();
        const QByteArray payload = reply->readAll();
        const auto error = reply->error();
        const QString errorText = reply->errorString();
        reply->deleteLater();
        if (m_cancelled) {
            setState(QStringLiteral("idle"));
            return;
        }
        if (error != QNetworkReply::NoError) {
            fail(QStringLiteral("Update check failed: %1").arg(errorText));
            return;
        }
        handler(payload);
    });
}

void UpdateService::processRelease(const QByteArray &payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(QStringLiteral("GitHub returned an invalid release description."));
        return;
    }

    const QJsonObject release = document.object();
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString() == QLatin1String(kManifestName)) {
            requestManifest(QUrl(asset.value(QStringLiteral("browser_download_url")).toString()),
                            payload);
            return;
        }
    }

    QString downloadUrl;
    QString digest;
    qint64 size = -1;
    for (const QJsonValue &value : assets) {
        const QJsonObject asset = value.toObject();
        if (asset.value(QStringLiteral("name")).toString() == QLatin1String(kWindowsArchive)) {
            downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
            digest = digestValue(asset.value(QStringLiteral("digest")).toString());
            size = asset.value(QStringLiteral("size")).toInteger(-1);
            break;
        }
    }

    applyReleaseData(release.value(QStringLiteral("tag_name")).toString(),
                     release.value(QStringLiteral("target_commitish")).toString(),
                     release.value(QStringLiteral("body")).toString(),
                     QUrl(release.value(QStringLiteral("html_url")).toString()),
                     QUrl(downloadUrl), digest, size);
}

void UpdateService::requestManifest(const QUrl &url, const QByteArray &fallbackRelease)
{
    startRequest(url, [this, fallbackRelease](QByteArray payload) {
        processManifest(payload, fallbackRelease);
    });
}

void UpdateService::processManifest(const QByteArray &payload,
                                    const QByteArray &fallbackRelease)
{
    QJsonParseError error;
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(payload, &error);
    const QJsonDocument releaseDocument = QJsonDocument::fromJson(fallbackRelease);
    if (error.error != QJsonParseError::NoError || !manifestDocument.isObject()
        || !releaseDocument.isObject()) {
        fail(QStringLiteral("The update manifest is invalid."));
        return;
    }

    const QJsonObject manifest = manifestDocument.object();
    const QJsonObject release = releaseDocument.object();
    if (manifest.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        fail(QStringLiteral("The update manifest version is not supported."));
        return;
    }
    applyReleaseData(manifest.value(QStringLiteral("version")).toString(),
                     manifest.value(QStringLiteral("commit")).toString(),
                     manifest.value(QStringLiteral("notes")).toString(
                         release.value(QStringLiteral("body")).toString()),
                     QUrl(manifest.value(QStringLiteral("releasePageUrl")).toString(
                         release.value(QStringLiteral("html_url")).toString())),
                     QUrl(manifest.value(QStringLiteral("downloadUrl")).toString()),
                     digestValue(manifest.value(QStringLiteral("sha256")).toString()),
                     manifest.value(QStringLiteral("size")).toInteger(-1));
}

void UpdateService::applyReleaseData(const QString &version, const QString &commit,
                                     const QString &notes, const QUrl &pageUrl,
                                     const QUrl &downloadUrl, const QString &sha256,
                                     qint64 size)
{
    m_latestVersion = normalizedVersion(version);
    m_latestCommit = commit.trimmed();
    m_releaseNotes = notes.trimmed();
    m_releasePageUrl = pageUrl;
    m_downloadUrl = downloadUrl;
    m_expectedSha256 = sha256;
    m_expectedSize = size;
    m_updateAvailable = isNewer(m_latestVersion, m_latestCommit);
    QSettings().setValue(QStringLiteral("updates/lastCheckUtc/%1").arg(m_requestedChannel),
                         QDateTime::currentDateTimeUtc());
    emit updateChanged();

    constexpr qint64 maximumPackageSize = 1024LL * 1024LL * 1024LL;
    if (m_updateAvailable
        && (!validPackageUrl(m_downloadUrl) || !validSha256(m_expectedSha256)
            || m_expectedSize <= 0 || m_expectedSize > maximumPackageSize)) {
        fail(QStringLiteral("The release is missing a verified Windows update package."));
        return;
    }

    setState(m_updateAvailable ? QStringLiteral("available")
                               : QStringLiteral("upToDate"));
    if (m_updateAvailable || !m_silent)
        emit interactionRequested();
}

bool UpdateService::isNewer(const QString &version, const QString &commit) const
{
    if (m_requestedChannel == QStringLiteral("development")) {
        const QString current = currentCommit();
        if (!commit.isEmpty() && current != QStringLiteral("unknown"))
            return !commit.startsWith(current) && !current.startsWith(commit);
        return !version.isEmpty() && version != currentVersion();
    }

    const QVersionNumber latest = QVersionNumber::fromString(normalizedVersion(version));
    const QVersionNumber current = QVersionNumber::fromString(normalizedVersion(currentVersion()));
    return !latest.isNull() && QVersionNumber::compare(latest, current) > 0;
}

void UpdateService::downloadAndInstall()
{
    if (!m_updateAvailable || busy())
        return;
    if (m_documents && m_documents->hasDirtyDocuments()) {
        fail(QStringLiteral("Save or close all modified maps before installing the update."));
        return;
    }

    const QString updateDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/DewralMapEditorUpdates");
    if (!QDir().mkpath(updateDir)) {
        fail(QStringLiteral("The temporary update directory could not be created."));
        return;
    }

    const QString archivePath = updateDir + QStringLiteral("/DME-update.zip");
    m_downloadFile.setFileName(archivePath + QStringLiteral(".part"));
    if (!m_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(QStringLiteral("The update package could not be written."));
        return;
    }

    m_downloadHash.reset();
    m_cancelled = false;
    m_downloadProgress = 0.0;
    emit downloadProgressChanged();
    setState(QStringLiteral("downloading"));
    m_reply = m_network.get(githubRequest(m_downloadUrl));

    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (!m_reply)
            return;
        const QByteArray chunk = m_reply->readAll();
        if (m_downloadFile.write(chunk) != chunk.size()) {
            m_reply->abort();
            return;
        }
        m_downloadHash.addData(chunk);
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_downloadProgress = qBound(0.0, double(received) / double(total), 1.0);
            emit downloadProgressChanged();
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, archivePath]() {
        QNetworkReply *reply = m_reply;
        if (!reply)
            return;
        m_reply.clear();
        const auto error = reply->error();
        const QString errorText = reply->errorString();
        reply->deleteLater();
        m_downloadFile.close();
        if (m_cancelled) {
            QFile::remove(m_downloadFile.fileName());
            setState(QStringLiteral("idle"));
            return;
        }
        if (error != QNetworkReply::NoError) {
            QFile::remove(m_downloadFile.fileName());
            fail(QStringLiteral("Update download failed: %1").arg(errorText));
            return;
        }

        const QFileInfo partial(m_downloadFile.fileName());
        if (m_expectedSize >= 0 && partial.size() != m_expectedSize) {
            QFile::remove(partial.filePath());
            fail(QStringLiteral("The downloaded update has an unexpected size."));
            return;
        }
        const QString actual = QString::fromLatin1(m_downloadHash.result().toHex());
        if (actual.compare(m_expectedSha256, Qt::CaseInsensitive) != 0) {
            QFile::remove(partial.filePath());
            fail(QStringLiteral("The downloaded update failed SHA-256 verification."));
            return;
        }

        QFile::remove(archivePath);
        if (!QFile::rename(partial.filePath(), archivePath)) {
            fail(QStringLiteral("The verified update package could not be finalized."));
            return;
        }
        setState(QStringLiteral("installing"));
        if (!launchUpdater(archivePath))
            return;
        QCoreApplication::quit();
    });
}

bool UpdateService::launchUpdater(const QString &archivePath)
{
#ifdef Q_OS_WIN
    const QString applicationDir = QCoreApplication::applicationDirPath();
    const QString source = applicationDir + QStringLiteral("/DMEUpdater.exe");
    if (!QFileInfo::exists(source)) {
        fail(QStringLiteral("DMEUpdater.exe is missing from the application folder."));
        return false;
    }

    const QString runtimePath = QFileInfo(archivePath).absolutePath()
        + QStringLiteral("/runtime-%1").arg(QCoreApplication::applicationPid());
    QDir runtime(runtimePath);
    if (runtime.exists() && !runtime.removeRecursively()) {
        fail(QStringLiteral("The previous temporary updater runtime could not be removed."));
        return false;
    }
    if (!QDir().mkpath(runtimePath + QStringLiteral("/platforms"))) {
        fail(QStringLiteral("The temporary updater runtime could not be created."));
        return false;
    }

    auto copyRuntimeFile = [](const QString &from, const QString &to,
                              bool required = true) {
        if (!QFileInfo::exists(from))
            return !required;
        QFile::remove(to);
        return QFile::copy(from, to);
    };
    const QString tempUpdater = runtimePath + QStringLiteral("/DMEUpdater.exe");
    const QStringList requiredFiles{
        QStringLiteral("DMEUpdater.exe"), QStringLiteral("Qt6Core.dll"),
        QStringLiteral("Qt6Gui.dll"), QStringLiteral("Qt6Widgets.dll")
    };
    for (const QString &file : requiredFiles) {
        if (!copyRuntimeFile(applicationDir + QLatin1Char('/') + file,
                             runtimePath + QLatin1Char('/') + file)) {
            runtime.removeRecursively();
            fail(QStringLiteral("The updater runtime is incomplete: %1 is missing.").arg(file));
            return false;
        }
    }
    if (!copyRuntimeFile(applicationDir + QStringLiteral("/platforms/qwindows.dll"),
                         runtimePath + QStringLiteral("/platforms/qwindows.dll"))) {
        runtime.removeRecursively();
        fail(QStringLiteral("The updater runtime is missing the Windows platform plugin."));
        return false;
    }
    const QStringList optionalFiles{
        QStringLiteral("libgcc_s_seh-1.dll"), QStringLiteral("libstdc++-6.dll"),
        QStringLiteral("libwinpthread-1.dll"), QStringLiteral("D3Dcompiler_47.dll")
    };
    for (const QString &file : optionalFiles)
        copyRuntimeFile(applicationDir + QLatin1Char('/') + file,
                        runtimePath + QLatin1Char('/') + file, false);

    const QStringList arguments{
        QStringLiteral("--pid"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("--archive"), QDir::toNativeSeparators(archivePath),
        QStringLiteral("--target"), QDir::toNativeSeparators(applicationDir),
        QStringLiteral("--exe"), QStringLiteral("DME.exe"),
        QStringLiteral("--sha256"), m_expectedSha256
    };
    if (!QProcess::startDetached(tempUpdater, arguments, runtimePath)) {
        fail(QStringLiteral("The updater process could not be started."));
        return false;
    }
    return true;
#else
    Q_UNUSED(archivePath)
    fail(QStringLiteral("Automatic installation is currently available only on Windows."));
    return false;
#endif
}

void UpdateService::cancel()
{
    m_cancelled = true;
    if (m_reply)
        m_reply->abort();
    if (m_downloadFile.isOpen())
        m_downloadFile.close();
    if (!m_downloadFile.fileName().isEmpty())
        QFile::remove(m_downloadFile.fileName());
    setState(QStringLiteral("idle"));
}

void UpdateService::openReleasePage() const
{
    if (m_releasePageUrl.isValid())
        QDesktopServices::openUrl(m_releasePageUrl);
}

void UpdateService::setState(const QString &state, const QString &error)
{
    m_state = state;
    m_errorString = error;
    emit stateChanged();
}

void UpdateService::fail(const QString &message)
{
    setState(QStringLiteral("error"), message);
    if (!m_silent)
        emit interactionRequested();
}

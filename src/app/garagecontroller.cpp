// Implementation of the garage settings and file handling (role: app).

#include "garagecontroller.h"

#include "attachmentstore.h"
#include "database.h"
#include "format.h"
#include "settingsrepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QUuid>

#include <KLocalizedString>

#include <cstdio>

namespace garagelog::app {

using core::SettingsRepository;

namespace {

/**
 * true if @p path is empty or already a GarageLog garage; otherwise sets a
 * translated @p error. Database::open() refuses the other cases as well,
 * this only gives the user a readable message.
 */
bool isGarageOrEmpty(const QString &path, QString &error)
{
    using Probe = core::Database::Probe;
    QString detail;
    switch (core::Database::probe(path, &detail)) {
    case Probe::Empty:
    case Probe::Garage:
        return true;
    case Probe::Missing:
        error = i18n("%1 does not exist", path);
        return false;
    case Probe::Foreign:
        error = i18n("%1 is not a GarageLog garage", path);
        return false;
    case Probe::NotSqlite:
        error = i18n("%1 is not an SQLite database", path);
        return false;
    }
    return false;
}

} // namespace

QString localPath(const QString &urlOrPath)
{
    const QUrl url(urlOrPath);
    return url.isLocalFile() ? url.toLocalFile() : urlOrPath;
}

GarageController::GarageController(core::Database &database, Format &format, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_format(format)
{
    loadSettings();
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &GarageController::checkFile);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &GarageController::checkFile);
    watchFile();
    checkFile();
}

QString GarageController::filePath() const
{
    return m_database.filePath();
}

QString GarageController::folderUrl() const
{
    return QUrl::fromLocalFile(QFileInfo(m_database.filePath()).absolutePath()).toString();
}

QString GarageController::distanceUnit() const
{
    return m_distanceUnit;
}

QString GarageController::volumeUnit() const
{
    return m_volumeUnit;
}

QString GarageController::currency() const
{
    return m_currency;
}

QStringList GarageController::currencies() const
{
    return {QStringLiteral("EUR"), QStringLiteral("USD"), QStringLiteral("GBP"), QStringLiteral("CHF"), QStringLiteral("CAD"),
            QStringLiteral("AUD"), QStringLiteral("PLN"), QStringLiteral("CZK"), QStringLiteral("SEK"), QStringLiteral("NOK"),
            QStringLiteral("DKK"), QStringLiteral("HUF")};
}

QString GarageController::errorString() const
{
    return m_error;
}

QString GarageController::fileState() const
{
    return m_fileState;
}

QString GarageController::startupNotice() const
{
    return m_startupNotice;
}

void GarageController::setStartupNotice(const QString &notice)
{
    m_startupNotice = notice;
}

void GarageController::checkFile()
{
    using State = core::Database::FileState;
    QString state;
    switch (m_database.fileState()) {
    case State::Missing:
        state = QStringLiteral("missing");
        break;
    case State::ReadOnly:
        state = QStringLiteral("readOnly");
        break;
    case State::Ok:
    case State::Closed:
        state = QStringLiteral("ok");
        break;
    }
    if (state != m_fileState) {
        m_fileState = state;
        Q_EMIT fileStateChanged();
    }
    // A watched file drops out of the watcher once it is deleted; re-adding
    // it here picks it up again after it was restored.
    watchFile();
}

void GarageController::watchFile()
{
    const QString file = m_database.filePath();
    const QStringList wanted = file.isEmpty() ? QStringList() : QStringList{file, QFileInfo(file).absolutePath()};
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (QSet<QString>(watched.begin(), watched.end()) == QSet<QString>(wanted.begin(), wanted.end())) {
        return;
    }
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }
    for (const QString &path : wanted) {
        // Fails quietly for a missing file; the folder still reports its return.
        if (QFileInfo::exists(path)) {
            m_watcher.addPath(path);
        }
    }
}

void GarageController::loadSettings()
{
    SettingsRepository settings(m_database);
    m_distanceUnit = settings.value(SettingsRepository::distanceUnitKey, QStringLiteral("km"));
    m_volumeUnit = settings.value(SettingsRepository::volumeUnitKey, QStringLiteral("l"));
    m_currency = settings.value(SettingsRepository::currencyKey, QStringLiteral("EUR"));
    m_format.setUnits(m_distanceUnit, m_volumeUnit, m_currency);
    // Format falls back on unknown values; show what is actually used.
    m_distanceUnit = m_format.distanceUnit();
    m_volumeUnit = m_format.volumeUnit();
    m_currency = m_format.currency();
    Q_EMIT settingsChanged();
}

bool GarageController::writeSetting(const QString &key, const QString &value)
{
    SettingsRepository settings(m_database);
    if (!settings.setValue(key, value)) {
        setErrorString(settings.errorString());
        return false;
    }
    setErrorString({});
    loadSettings();
    return true;
}

void GarageController::setDistanceUnit(const QString &unit)
{
    if (unit != m_distanceUnit) {
        writeSetting(SettingsRepository::distanceUnitKey, unit);
    }
}

void GarageController::setVolumeUnit(const QString &unit)
{
    if (unit != m_volumeUnit) {
        writeSetting(SettingsRepository::volumeUnitKey, unit);
    }
}

void GarageController::setCurrency(const QString &code)
{
    if (code != m_currency) {
        writeSetting(SettingsRepository::currencyKey, code);
    }
}

bool GarageController::switchTo(const QString &path, core::Database::OpenMode mode)
{
    const QString previous = m_database.filePath();
    if (QFileInfo(path).absoluteFilePath() == QFileInfo(previous).absoluteFilePath()) {
        setErrorString({});
        return true;
    }
    // open() keeps the previous garage open if the new one fails.
    if (!m_database.open(path, mode)) {
        setErrorString(m_database.errorString());
        return false;
    }
    setErrorString({});
    loadSettings();
    checkFile();
    Q_EMIT garageChanged();
    return true;
}

bool GarageController::openGarage(const QString &urlOrPath)
{
    const QString path = localPath(urlOrPath);
    if (!QFileInfo(path).isFile()) {
        setErrorString(i18n("%1 does not exist", path));
        return false;
    }
    QString error;
    if (!isGarageOrEmpty(path, error)) {
        setErrorString(error);
        return false;
    }
    return switchTo(path, core::Database::OpenMode::MustExist);
}

bool GarageController::newGarage(const QString &urlOrPath)
{
    const QString path = localPath(urlOrPath);
    if (QFileInfo::exists(path)) {
        setErrorString(i18n("%1 already exists; use “Open Garage” for existing files", path));
        return false;
    }
    return switchTo(path, core::Database::OpenMode::CreateIfMissing);
}

bool GarageController::saveGarageAs(const QString &urlOrPath)
{
    const QString target = QFileInfo(localPath(urlOrPath)).absoluteFilePath();
    if (target == QFileInfo(m_database.filePath()).absoluteFilePath()) {
        setErrorString(i18n("Please choose a different file than the open garage"));
        return false;
    }
    if (QFileInfo(target).isDir()) {
        setErrorString(i18n("%1 is a folder", target));
        return false;
    }
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        setErrorString(i18n("Cannot create folder for %1", target));
        return false;
    }
    // VACUUM INTO refuses existing files, so it writes a temporary file next
    // to the target that replaces the target only once it is complete: a
    // full disk must not destroy the file the user chose to overwrite.
    const QString temporary = target + QStringLiteral(".") + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".tmp");
    QSqlQuery vacuum(m_database.connection());
    vacuum.prepare(QStringLiteral("VACUUM INTO ?"));
    vacuum.addBindValue(temporary);
    if (!vacuum.exec()) {
        QFile::remove(temporary);
        setErrorString(i18n("Cannot copy garage: %1", vacuum.lastError().text()));
        return false;
    }
    QFile::setPermissions(temporary, QFile::ReadOwner | QFile::WriteOwner);

    // Attachments go next to the copy, as AttachmentStore expects them there.
    // Only files this garage refers to: the folder may be shared with other
    // garages in the same directory, whose files must not travel along.
    const core::AttachmentStore store(m_database);
    QStringList names;
    QSqlQuery query(m_database.connection());
    if (!query.exec(QStringLiteral("SELECT stored_name FROM attachments"))) {
        QFile::remove(temporary);
        setErrorString(i18n("Cannot copy garage: %1", query.lastError().text()));
        return false;
    }
    while (query.next()) {
        names.append(query.value(0).toString());
    }
    const QDir targetDir(QFileInfo(target).absoluteDir().filePath(core::AttachmentStore::directoryName));
    if (!names.isEmpty() && !targetDir.exists()) {
        if (!QDir().mkpath(targetDir.path())) {
            QFile::remove(temporary);
            setErrorString(i18n("Cannot create %1", targetDir.path()));
            return false;
        }
        QFile::setPermissions(targetDir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    for (const QString &name : names) {
        const QString from = store.filePath(name);
        // Unsafe names (crafted garage) and missing files are skipped; the
        // row stays, as it does in the open garage.
        if (from.isEmpty() || !QFileInfo::exists(from)) {
            continue;
        }
        // Same uuid name = same file (e.g. saving twice into one folder).
        if (!targetDir.exists(name) && !QFile::copy(from, targetDir.filePath(name))) {
            QFile::remove(temporary);
            setErrorString(i18n("Cannot copy attachment %1", name));
            return false;
        }
    }

    // A journal left by an earlier file of that name would be applied to
    // the new copy by SQLite and corrupt it.
    QFile::remove(target + QStringLiteral("-journal"));
    // std::rename replaces an existing target atomically (POSIX), unlike
    // QFile::rename, which refuses existing files.
    if (std::rename(QFile::encodeName(temporary).constData(), QFile::encodeName(target).constData()) != 0) {
        QFile::remove(temporary);
        setErrorString(i18n("Cannot replace %1", target));
        return false;
    }
    return switchTo(target, core::Database::OpenMode::MustExist);
}

void GarageController::setErrorString(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorStringChanged();
    }
}

} // namespace garagelog::app

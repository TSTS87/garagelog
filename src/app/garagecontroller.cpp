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

namespace garagelog::app {

using core::SettingsRepository;

namespace {

/**
 * true if @p path is empty or already a GarageLog garage. Checked on a
 * separate read-only connection, so a foreign SQLite file is never
 * touched by our migrations.
 */
bool isGarageOrEmpty(const QString &path, QString &error)
{
    if (QFileInfo(path).size() == 0) {
        return true;
    }
    const QString name = QStringLiteral("garagelog-check-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open()) {
            error = QStringLiteral("Cannot open %1: %2").arg(path, db.lastError().text());
        } else {
            QSqlQuery q(db);
            if (!q.exec(QStringLiteral("SELECT COUNT(*), SUM(name = 'schema_version') FROM sqlite_master WHERE type = 'table'")) || !q.next()) {
                error = QStringLiteral("%1 is not an SQLite database").arg(path);
            } else if (q.value(0).toInt() > 0 && q.value(1).toInt() == 0) {
                error = QStringLiteral("%1 is not a GarageLog garage").arg(path);
            } else {
                ok = true;
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
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

bool GarageController::switchTo(const QString &path)
{
    const QString previous = m_database.filePath();
    if (QFileInfo(path).absoluteFilePath() == QFileInfo(previous).absoluteFilePath()) {
        setErrorString({});
        return true;
    }
    if (!m_database.open(path)) {
        const QString error = m_database.errorString();
        // open() has closed the previous file already; without reopening it
        // every model would hit a closed connection.
        m_database.open(previous);
        setErrorString(error);
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
        setErrorString(QStringLiteral("%1 does not exist").arg(path));
        return false;
    }
    QString error;
    if (!isGarageOrEmpty(path, error)) {
        setErrorString(error);
        return false;
    }
    return switchTo(path);
}

bool GarageController::newGarage(const QString &urlOrPath)
{
    const QString path = localPath(urlOrPath);
    if (QFileInfo::exists(path)) {
        setErrorString(QStringLiteral("%1 already exists; use “Open Garage” for existing files").arg(path));
        return false;
    }
    return switchTo(path);
}

bool GarageController::saveGarageAs(const QString &urlOrPath)
{
    const QString target = QFileInfo(localPath(urlOrPath)).absoluteFilePath();
    if (target == QFileInfo(m_database.filePath()).absoluteFilePath()) {
        setErrorString(QStringLiteral("Please choose a different file than the open garage"));
        return false;
    }
    if (QFileInfo(target).isDir()) {
        setErrorString(QStringLiteral("%1 is a folder").arg(target));
        return false;
    }
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        setErrorString(QStringLiteral("Cannot create folder for %1").arg(target));
        return false;
    }
    // VACUUM INTO refuses existing files; the save dialog already asked
    // whether to overwrite.
    if (QFileInfo::exists(target) && !QFile::remove(target)) {
        setErrorString(QStringLiteral("Cannot replace %1").arg(target));
        return false;
    }
    QSqlQuery vacuum(m_database.connection());
    vacuum.prepare(QStringLiteral("VACUUM INTO ?"));
    vacuum.addBindValue(target);
    if (!vacuum.exec()) {
        setErrorString(QStringLiteral("Cannot copy garage: %1").arg(vacuum.lastError().text()));
        return false;
    }

    // Attachments go next to the copy, as AttachmentStore expects them there.
    const QDir source(core::AttachmentStore(m_database).directory());
    const QDir targetDir(QFileInfo(target).absoluteDir().filePath(core::AttachmentStore::directoryName));
    const QStringList files = source.exists() ? source.entryList(QDir::Files) : QStringList();
    if (!files.isEmpty() && !QDir().mkpath(targetDir.path())) {
        setErrorString(QStringLiteral("Cannot create %1").arg(targetDir.path()));
        return false;
    }
    for (const QString &name : files) {
        // Same uuid name = same file (e.g. saving twice into one folder).
        if (!targetDir.exists(name) && !QFile::copy(source.filePath(name), targetDir.filePath(name))) {
            setErrorString(QStringLiteral("Cannot copy attachment %1").arg(name));
            return false;
        }
    }
    return switchTo(target);
}

void GarageController::setErrorString(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorStringChanged();
    }
}

} // namespace garagelog::app

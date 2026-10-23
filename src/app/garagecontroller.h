// Garage settings and garage file handling for QML (role: app).
// Reads/writes the per-garage units and currency (settings table) and
// applies them to Format; opens another garage file, creates a new one or
// saves a copy ("save as"). Exposed to QML as singleton "Garage".

#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

namespace garagelog::core {
class Database;
}

namespace garagelog::app {

class Format;

/**
 * @brief Settings page backend: units, currency and which garage file is open.
 *
 * Switching the garage reopens the shared core::Database in place, so all
 * models keep their reference and only need a reload (garageChanged()).
 * If the new file cannot be opened, the previous one is reopened.
 * Paths may be given as file:// URLs (from FileDialog) or plain paths.
 *
 * It also watches the open file (M15): if it is deleted, moved or becomes
 * write-protected, fileState changes and the window shows a warning; the
 * data is still in the open connection, so "save as" rescues it.
 */
class GarageController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY garageChanged)
    /** Directory of the garage file as file:// URL, the start folder for dialogs. */
    Q_PROPERTY(QString folderUrl READ folderUrl NOTIFY garageChanged)
    Q_PROPERTY(QString distanceUnit READ distanceUnit WRITE setDistanceUnit NOTIFY settingsChanged)
    Q_PROPERTY(QString volumeUnit READ volumeUnit WRITE setVolumeUnit NOTIFY settingsChanged)
    Q_PROPERTY(QString currency READ currency WRITE setCurrency NOTIFY settingsChanged)
    /** ISO codes offered in the currency picker. */
    Q_PROPERTY(QStringList currencies READ currencies CONSTANT)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    /** "ok", "missing" or "readOnly" (core::Database::FileState); "ok" also while no garage is open. */
    Q_PROPERTY(QString fileState READ fileState NOTIFY fileStateChanged)

public:
    /** @param database The app's open database; reopened in place on a switch. @param format Receives the units. */
    GarageController(core::Database &database, Format &format, QObject *parent = nullptr);

    QString filePath() const;
    QString folderUrl() const;
    QString distanceUnit() const;
    void setDistanceUnit(const QString &unit);
    QString volumeUnit() const;
    void setVolumeUnit(const QString &unit);
    QString currency() const;
    void setCurrency(const QString &code);
    QStringList currencies() const;
    QString errorString() const;
    QString fileState() const;

    /**
     * @brief Re-checks the garage file on disk and updates fileState.
     *
     * Called by the file watcher and by the window when it is activated, as
     * a watcher can miss changes (e.g. on network file systems).
     */
    Q_INVOKABLE void checkFile();

    /** Reads units and currency of the open garage and applies them to Format (defaults km, l, EUR). */
    void loadSettings();

    /**
     * @brief Opens an existing garage file.
     * @return false if it does not exist, is not a GarageLog garage (an
     *         SQLite file with other tables) or cannot be opened.
     */
    Q_INVOKABLE bool openGarage(const QString &urlOrPath);

    /** Creates and opens a new, empty garage. false if the file already exists. */
    Q_INVOKABLE bool newGarage(const QString &urlOrPath);

    /**
     * @brief Copies the garage (database and attachments/) to a new place and continues there.
     *
     * The database is copied with SQLite's VACUUM INTO, which gives a
     * consistent copy while the file is open. An existing target file is
     * replaced (the save dialog has asked); the open garage itself cannot
     * be the target.
     */
    Q_INVOKABLE bool saveGarageAs(const QString &urlOrPath);

Q_SIGNALS:
    /** Another garage file is open now; every model must reload. */
    void garageChanged();
    void settingsChanged();
    void errorStringChanged();
    void fileStateChanged();

private:
    /** Opens @p path, falling back to the previous file on failure. */
    bool switchTo(const QString &path);
    bool writeSetting(const QString &key, const QString &value);
    void setErrorString(const QString &error);
    /** Points the watcher at the open file and its folder (the folder sees deletes and renames). */
    void watchFile();

    core::Database &m_database;
    Format &m_format;
    QString m_distanceUnit;
    QString m_volumeUnit;
    QString m_currency;
    QString m_error;
    QString m_fileState = QStringLiteral("ok");
    QFileSystemWatcher m_watcher;
};

/** Local path of a file:// URL; anything else is returned unchanged. */
QString localPath(const QString &urlOrPath);

} // namespace garagelog::app

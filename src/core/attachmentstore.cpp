// Implementation of the attachment file store (role: core).

#include "attachmentstore.h"

#include "database.h"
#include "dbvalues.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace garagelog::core {

using namespace db;

namespace {

const QString selectColumns = QStringLiteral(
    "SELECT id, service_entry_id, filename, stored_name, mime, size_bytes, created_at FROM attachments");

Attachment readAttachment(const QSqlQuery &query)
{
    Attachment a;
    a.id = query.value(0).toString();
    a.serviceEntryId = query.value(1).toString();
    a.filename = query.value(2).toString();
    a.storedName = query.value(3).toString();
    a.mime = query.value(4).toString();
    a.sizeBytes = query.value(5).toLongLong();
    a.createdAt = fromDbTimestamp(query.value(6));
    return a;
}

} // namespace

AttachmentStore::AttachmentStore(const Database &database)
    : m_database(database)
{
}

QString AttachmentStore::directory() const
{
    return QFileInfo(m_database.filePath()).absoluteDir().filePath(directoryName);
}

QString AttachmentStore::filePath(const Attachment &attachment) const
{
    return filePath(attachment.storedName);
}

QString AttachmentStore::filePath(const QString &storedName) const
{
    if (!isSafeStoredName(storedName)) {
        return {};
    }
    const QString path = QDir(directory()).filePath(storedName);
    // A planted symlink (attachments/x.pdf -> ~/.ssh/id_rsa) would let an
    // export or "open" reach a file outside the garage.
    const QFileInfo info(path);
    if (info.isSymLink()) {
        const QString dir = QFileInfo(directory()).canonicalFilePath();
        const QString target = info.canonicalFilePath();
        if (dir.isEmpty() || !target.startsWith(dir + QLatin1Char('/'))) {
            return {};
        }
    }
    return path;
}

bool AttachmentStore::isSafeStoredName(const QString &storedName)
{
    return !storedName.isEmpty() && !storedName.contains(QLatin1Char('/')) && !storedName.contains(QLatin1Char('\\'))
        && !storedName.startsWith(QLatin1Char('.')) && !storedName.contains(QChar(0));
}

std::optional<Attachment> AttachmentStore::add(const QString &serviceEntryId, const QString &sourcePath)
{
    m_error.clear();
    const QFileInfo source(sourcePath);
    if (!source.isFile() || !source.isReadable()) {
        m_error = QStringLiteral("Cannot read file %1").arg(sourcePath);
        return std::nullopt;
    }
    if (!QDir().mkpath(directory())) {
        m_error = QStringLiteral("Cannot create directory %1").arg(directory());
        return std::nullopt;
    }

    Attachment a;
    a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.serviceEntryId = serviceEntryId;
    a.filename = source.fileName();
    // A fresh uuid instead of the attachment id keeps the file name
    // independent from the row, should rows ever be re-keyed on import.
    const QString suffix = source.suffix().toLower();
    a.storedName = QUuid::createUuid().toString(QUuid::WithoutBraces) + (suffix.isEmpty() ? QString() : QLatin1Char('.') + suffix);
    a.mime = QMimeDatabase().mimeTypeForFile(source).name();
    a.sizeBytes = source.size();
    a.createdAt = nowUtc();

    const QString target = filePath(a);
    if (!QFile::copy(source.absoluteFilePath(), target)) {
        m_error = QStringLiteral("Cannot copy %1 to %2").arg(sourcePath, target);
        return std::nullopt;
    }

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO attachments (id, service_entry_id, filename, stored_name, mime, size_bytes, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(a.id);
    query.addBindValue(a.serviceEntryId);
    query.addBindValue(a.filename);
    query.addBindValue(a.storedName);
    query.addBindValue(nullIfEmpty(a.mime));
    query.addBindValue(a.sizeBytes);
    query.addBindValue(toDbTimestamp(a.createdAt));
    // An unknown service entry fails the foreign key; then the copy must go.
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot record attachment: %1").arg(query.lastError().text());
        QFile::remove(target);
        return std::nullopt;
    }
    return a;
}

QList<Attachment> AttachmentStore::byServiceEntry(const QString &serviceEntryId)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE service_entry_id = ? ORDER BY created_at, filename"));
    query.addBindValue(serviceEntryId);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot list attachments: %1").arg(query.lastError().text());
        return {};
    }
    QList<Attachment> result;
    while (query.next()) {
        result.append(readAttachment(query));
    }
    return result;
}

std::optional<Attachment> AttachmentStore::byId(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read attachment: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        m_error = QStringLiteral("No attachment with id %1").arg(id);
        return std::nullopt;
    }
    return readAttachment(query);
}

QSet<QString> AttachmentStore::entriesWithAttachments(const QString &vehicleId)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "SELECT DISTINCT a.service_entry_id FROM attachments a "
        "JOIN service_entries s ON s.id = a.service_entry_id WHERE s.vehicle_id = ?"));
    query.addBindValue(vehicleId);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot list attachments: %1").arg(query.lastError().text());
        return {};
    }
    QSet<QString> result;
    while (query.next()) {
        result.insert(query.value(0).toString());
    }
    return result;
}

bool AttachmentStore::remove(const QString &id)
{
    m_error.clear();
    const auto attachment = byId(id);
    if (!attachment) {
        return false;
    }
    // Row first: a leftover file is harmless, a row pointing to a deleted
    // file would show an attachment that cannot be opened.
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("DELETE FROM attachments WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot delete attachment: %1").arg(query.lastError().text());
        return false;
    }
    removeFiles({attachment->storedName});
    return true;
}

QStringList AttachmentStore::storedNamesOf(const QString &serviceEntryId)
{
    QStringList names;
    for (const Attachment &a : byServiceEntry(serviceEntryId)) {
        names.append(a.storedName);
    }
    return names;
}

bool AttachmentStore::removeFiles(const QStringList &storedNames)
{
    bool ok = true;
    for (const QString &name : storedNames) {
        // Never follow a stored name out of the directory, e.g. from a
        // manipulated garage ("../garage.sqlite"); filePath() returns "" then.
        const QString path = filePath(name);
        if (path.isEmpty()) {
            continue;
        }
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            m_error = QStringLiteral("Cannot delete attachment file %1").arg(path);
            ok = false;
        }
    }
    return ok;
}

QString AttachmentStore::errorString() const
{
    return m_error;
}

} // namespace garagelog::core

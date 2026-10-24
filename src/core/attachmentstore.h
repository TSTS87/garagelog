// Files attached to service entries (role: core).
// Copies a chosen file into <garage directory>/attachments/<uuid>.<ext>
// and records it in the attachments table (ADR-005); deletes row and file
// together. The garage directory is the one of the open Database file.

#pragma once

#include "serviceentry.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Adds, lists and removes attachment files and their rows.
 *
 * The attachments/ directory is created on the first add(). Removing a
 * service entry removes its attachment rows via ON DELETE CASCADE; the
 * files are removed by ServiceRepository::remove() through removeFiles().
 * Methods returning bool/optional report problems in errorString().
 */
class AttachmentStore
{
public:
    /** Name of the directory next to the garage file. */
    static inline const QString directoryName = QStringLiteral("attachments");

    explicit AttachmentStore(const Database &database);

    /** Absolute path of the attachments directory of the open garage (may not exist yet). */
    QString directory() const;

    /**
     * @brief Absolute path of the stored copy of @p attachment, for opening or copying it.
     *
     * The stored name comes from the garage file, which may have been
     * crafted by someone else, so it is never trusted as a path.
     * @return "" if the stored name is not a plain file name or the file is
     *         a symlink that points out of the attachments directory.
     */
    QString filePath(const Attachment &attachment) const;

    /** @copydoc filePath(const Attachment &) const */
    QString filePath(const QString &storedName) const;

    /**
     * @brief True if @p storedName is a plain file name inside attachments/.
     *
     * Rejects empty names, path separators, a leading dot ("..", hidden
     * files) and NUL characters. Used for every stored name read from a
     * garage or an export before it becomes part of a path.
     */
    static bool isSafeStoredName(const QString &storedName);

    /**
     * @brief Copies @p sourcePath into the attachments directory and records it.
     *
     * The copy is named "<new uuid>.<lower-case suffix>" (no dot without
     * suffix), so names never collide and the original name may contain
     * anything; filename keeps the original name for display.
     *
     * @return The new row, or nullopt if the source is not a readable file,
     *         the copy fails or the entry does not exist (the copy is then
     *         removed again).
     */
    std::optional<Attachment> add(const QString &serviceEntryId, const QString &sourcePath);

    /** Attachments of one service entry, oldest first; empty on error (errorString set). */
    QList<Attachment> byServiceEntry(const QString &serviceEntryId);

    /** The attachment with @p id, or nullopt. */
    std::optional<Attachment> byId(const QString &id);

    /** Ids of the service entries of @p vehicleId that have at least one attachment (for the paperclip icon). */
    QSet<QString> entriesWithAttachments(const QString &vehicleId);

    /**
     * @brief Deletes one attachment: first the row, then the file.
     * @return false if the id is unknown or the row cannot be deleted. A
     *         file that cannot be deleted afterwards only leaves an orphan
     *         copy; that is reported in errorString() but returns true.
     */
    bool remove(const QString &id);

    /** Stored names of all attachments of @p serviceEntryId (read before deleting the entry). */
    QStringList storedNamesOf(const QString &serviceEntryId);

    /**
     * @brief Deletes stored copies by name; missing files are ignored.
     * @return false if at least one existing file could not be deleted.
     */
    bool removeFiles(const QStringList &storedNames);

    /** Reason for the last failed call. */
    QString errorString() const;

private:
    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core

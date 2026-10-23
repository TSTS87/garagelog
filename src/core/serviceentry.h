// Value types of the service side: ServiceEntry, ServiceCategory, Attachment (role: core).
// Plain data, no database access; ServiceRepository, CategoryRepository and
// AttachmentStore read and write them. Units: metres and cents (DATA_MODEL.md).

#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

#include <optional>

namespace garagelog::core {

/** Who did the work; stored in service_entries.performed_by (NULL, 'self' or 'shop'). */
enum class PerformedBy {
    Unknown, ///< NULL in the database, e.g. for insurance or tax
    Self,
    Shop,
};

/** Database value for @p who: "self", "shop", or an empty string for Unknown (stored as NULL). */
QString toDbString(PerformedBy who);

/** Parses service_entries.performed_by; empty/NULL text is Unknown, nullopt for text the schema does not allow. */
std::optional<PerformedBy> performedByFromDb(const QString &text);

/**
 * @brief One row of service_categories.
 *
 * Built-in categories store a stable key as name (e.g. "oil_change") that
 * the UI translates; user categories store the display name as typed.
 */
struct ServiceCategory {
    QString id;           ///< UUIDv4 without braces; fixed literals for built-ins (migration 0001)
    QString name;         ///< unique; key for built-ins, display text for user categories
    bool builtin = false; ///< true = shipped with the app, cannot be renamed or deleted
    QString icon;         ///< freedesktop icon name, empty = default icon

    bool operator==(const ServiceCategory &) const = default;
};

/**
 * @brief One row of service_entries: maintenance, repair or a recurring cost.
 *
 * id, createdAt and updatedAt are set by ServiceRepository; callers fill
 * the rest. Empty QStrings stand for NULL in the optional text columns.
 */
struct ServiceEntry {
    QString id;                       ///< UUIDv4 without braces; empty until created
    QString vehicleId;                ///< owning vehicle, required
    QString categoryId;               ///< service_categories.id, required
    QDate date;                       ///< day of the service, required
    std::optional<qint64> odometerM;  ///< metres; nullopt for odometer-independent costs (tax, insurance)
    QString description;              ///< required (not blank)
    qint64 costC = 0;                 ///< total cost in cents, >= 0
    PerformedBy performedBy = PerformedBy::Unknown;
    QString shopName;                 ///< workshop name, usually only with PerformedBy::Shop
    QString notes;
    QDateTime createdAt;              ///< UTC, whole seconds
    QDateTime updatedAt;              ///< UTC, whole seconds

    bool operator==(const ServiceEntry &) const = default;
};

/**
 * @brief One row of attachments: a file (receipt, photo) belonging to a service entry.
 *
 * The file itself lives in the attachments/ directory next to the garage
 * database under storedName (ADR-005); AttachmentStore manages both.
 */
struct Attachment {
    QString id;               ///< UUIDv4 without braces
    QString serviceEntryId;   ///< owning entry; the row is deleted with it (ON DELETE CASCADE)
    QString filename;         ///< original file name as chosen by the user
    QString storedName;       ///< "<uuid>.<ext>" inside attachments/
    QString mime;             ///< MIME type guessed from name and content, may be empty
    qint64 sizeBytes = 0;     ///< file size in bytes at the time it was attached
    QDateTime createdAt;      ///< UTC, whole seconds

    bool operator==(const Attachment &) const = default;
};

} // namespace garagelog::core

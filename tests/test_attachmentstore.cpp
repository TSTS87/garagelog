// Tests for AttachmentStore (role: tests).
// The garage lives in a temporary directory, so attachments/ is created
// next to it there and every file operation stays inside the temp folder.

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "attachmentstore.h"
#include "categoryrepository.h"
#include "database.h"
#include "servicerepository.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

/**
 * @brief Covers M07: copying attachments into the garage and deleting them with their entry.
 */
class TestAttachmentStore : public QObject
{
    Q_OBJECT

private:
    /** Writes a small file outside the garage directory and returns its path. */
    QString sourceFile(const QString &name, const QByteArray &content)
    {
        const QString path = m_sourceDir->filePath(name);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(content);
        }
        return path;
    }

    /** Creates a service entry for the test vehicle and returns its id. */
    QString newEntry()
    {
        ServiceRepository repo(*m_db);
        ServiceEntry e;
        e.vehicleId = m_vehicleId;
        e.categoryId = CategoryRepository(*m_db).byName(QStringLiteral("repair"))->id;
        e.date = QDate(2026, 10, 1);
        e.description = QStringLiteral("Clutch");
        repo.create(e);
        return e.id;
    }

private Q_SLOTS:
    void init()
    {
        m_garageDir = std::make_unique<QTemporaryDir>();
        m_sourceDir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_garageDir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));
        VehicleRepository vehicles(*m_db);
        Vehicle v;
        v.name = QStringLiteral("Golf");
        QVERIFY(vehicles.create(v));
        m_vehicleId = v.id;
    }

    void cleanup()
    {
        m_db.reset();
        m_garageDir.reset();
        m_sourceDir.reset();
    }

    /** add() copies the file to attachments/<uuid>.<ext>, keeps the original name and records size and MIME type. */
    void addCopiesFile()
    {
        AttachmentStore store(*m_db);
        const QString entryId = newEntry();
        const QString source = sourceFile(QStringLiteral("Rechnung März.PDF"), "%PDF-1.4 test");

        const auto a = store.add(entryId, source);
        QVERIFY2(a, qPrintable(store.errorString()));
        QCOMPARE(store.directory(), m_garageDir->filePath(QStringLiteral("attachments")));
        QCOMPARE(a->filename, QStringLiteral("Rechnung März.PDF"));
        QVERIFY(a->storedName.endsWith(QStringLiteral(".pdf")));
        QCOMPARE(a->sizeBytes, 13);
        QCOMPARE(a->mime, QStringLiteral("application/pdf"));

        QFile copy(store.filePath(*a));
        QVERIFY(copy.open(QIODevice::ReadOnly));
        QCOMPARE(copy.readAll(), QByteArray("%PDF-1.4 test"));
        QVERIFY(QFile::exists(source)); // the original stays where it was

        QCOMPARE(store.byServiceEntry(entryId), QList<Attachment>{*a});
        QCOMPARE(store.entriesWithAttachments(m_vehicleId), QSet<QString>{entryId});
    }

    /** Missing source files and unknown entries fail without leaving a copy behind. */
    void addFailures()
    {
        AttachmentStore store(*m_db);
        QVERIFY(!store.add(newEntry(), m_sourceDir->filePath(QStringLiteral("missing.jpg"))));
        QVERIFY(!store.add(QStringLiteral("no-such-entry"), sourceFile(QStringLiteral("a.jpg"), "x")));
        QVERIFY(QDir(store.directory()).isEmpty());
    }

    /** remove() deletes row and file of one attachment and leaves the others alone. */
    void removeOne()
    {
        AttachmentStore store(*m_db);
        const QString entryId = newEntry();
        const auto a = store.add(entryId, sourceFile(QStringLiteral("a.jpg"), "a"));
        const auto b = store.add(entryId, sourceFile(QStringLiteral("b.jpg"), "b"));
        QVERIFY(a && b);
        QVERIFY2(store.remove(a->id), qPrintable(store.errorString()));
        QVERIFY(!QFile::exists(store.filePath(*a)));
        QVERIFY(QFile::exists(store.filePath(*b)));
        QCOMPARE(store.byServiceEntry(entryId).size(), 1);
        QVERIFY(!store.remove(a->id));
    }

    /** Deleting the service entry removes its attachment rows and files. */
    void removedWithEntry()
    {
        AttachmentStore store(*m_db);
        const QString entryId = newEntry();
        const QString otherId = newEntry();
        const auto a = store.add(entryId, sourceFile(QStringLiteral("a.png"), "a"));
        const auto other = store.add(otherId, sourceFile(QStringLiteral("o.png"), "o"));
        QVERIFY(a && other);

        ServiceRepository services(*m_db);
        QVERIFY2(services.remove(entryId), qPrintable(services.errorString()));
        QVERIFY(store.byServiceEntry(entryId).isEmpty());
        QVERIFY(!QFile::exists(store.filePath(*a)));
        QVERIFY(QFile::exists(store.filePath(*other)));
    }

    /** removeFiles() refuses names that would leave the attachments directory. */
    void removeFilesStaysInside()
    {
        AttachmentStore store(*m_db);
        QVERIFY(store.removeFiles({QStringLiteral("../garage.sqlite"), QStringLiteral(".."), QString()}));
        QVERIFY(QFile::exists(m_garageDir->filePath(QStringLiteral("garage.sqlite"))));
    }

private:
    std::unique_ptr<QTemporaryDir> m_garageDir;
    std::unique_ptr<QTemporaryDir> m_sourceDir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
};

QTEST_GUILESS_MAIN(TestAttachmentStore)
#include "test_attachmentstore.moc"

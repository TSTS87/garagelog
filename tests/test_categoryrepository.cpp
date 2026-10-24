// Tests for CategoryRepository (role: tests).
// Each test opens a fresh garage in a temporary directory, so the 13 seeded
// built-in categories are always the starting point.

#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "categoryrepository.h"
#include "database.h"
#include "servicerepository.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

namespace {

/** A category with only a name set, as the UI creates it. */
ServiceCategory named(const QString &name)
{
    ServiceCategory c;
    c.name = name;
    return c;
}

} // namespace

/**
 * @brief Covers M07: listing, creating, renaming and protecting categories.
 */
class TestCategoryRepository : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** all() returns the 13 seeded built-ins in seed order, starting with oil_change and ending with other. */
    void builtinsInSeedOrder()
    {
        CategoryRepository repo(*m_db);
        const auto all = repo.all();
        QCOMPARE(all.size(), 13);
        QCOMPARE(all.first().name, QStringLiteral("oil_change"));
        QCOMPARE(all.last().name, QStringLiteral("other"));
        for (const auto &c : all) {
            QVERIFY(c.builtin);
        }
    }

    /** User categories are created non-builtin, trimmed, and listed after the built-ins by name. */
    void createUserCategory()
    {
        CategoryRepository repo(*m_db);
        ServiceCategory zeta = named(QStringLiteral("  Zeta wash "));
        ServiceCategory alpha = named(QStringLiteral("alpha polish"));
        alpha.builtin = true; // must be ignored
        QVERIFY2(repo.create(zeta), qPrintable(repo.errorString()));
        QVERIFY2(repo.create(alpha), qPrintable(repo.errorString()));
        QCOMPARE(zeta.name, QStringLiteral("Zeta wash"));
        QVERIFY(!alpha.builtin);

        const auto all = repo.all();
        QCOMPARE(all.size(), 15);
        QCOMPARE(all.at(13).name, QStringLiteral("alpha polish"));
        QCOMPARE(all.at(14).name, QStringLiteral("Zeta wash"));
        QCOMPARE(*repo.byId(zeta.id), zeta);
        QCOMPARE(repo.byName(QStringLiteral("tires"))->builtin, true);
    }

    /** Blank and duplicate names (also clashing with a built-in key, in any letter case, umlauts included) are refused. */
    void invalidNames()
    {
        CategoryRepository repo(*m_db);
        ServiceCategory blank = named(QStringLiteral("   "));
        QVERIFY(!repo.create(blank));
        ServiceCategory dup = named(QStringLiteral("tires"));
        QVERIFY(!repo.create(dup));
        QVERIFY(repo.errorString().contains(QStringLiteral("already exists")));
        ServiceCategory upper = named(QStringLiteral("TIRES"));
        QVERIFY(!repo.create(upper));
        ServiceCategory wash = named(QStringLiteral("Wäsche"));
        QVERIFY(repo.create(wash));
        ServiceCategory washLower = named(QStringLiteral("WÄSCHE"));
        QVERIFY(!repo.create(washLower));
    }

    /** Built-ins can be neither renamed nor deleted. */
    void builtinsAreProtected()
    {
        CategoryRepository repo(*m_db);
        auto tires = *repo.byName(QStringLiteral("tires"));
        tires.name = QStringLiteral("Reifen");
        QVERIFY(!repo.update(tires));
        QVERIFY(!repo.remove(tires.id));
        QVERIFY(repo.byName(QStringLiteral("tires")));
    }

    /** A user category can be renamed and deleted while unused. */
    void renameAndRemoveUserCategory()
    {
        CategoryRepository repo(*m_db);
        ServiceCategory c = named(QStringLiteral("Wash"));
        QVERIFY(repo.create(c));
        c.name = QStringLiteral("Car wash");
        QVERIFY2(repo.update(c), qPrintable(repo.errorString()));
        QCOMPARE(repo.byId(c.id)->name, QStringLiteral("Car wash"));
        QVERIFY2(repo.remove(c.id), qPrintable(repo.errorString()));
        QVERIFY(!repo.byId(c.id));
    }

    /** A user category used by a service entry cannot be deleted. */
    void usedCategoryIsKept()
    {
        CategoryRepository repo(*m_db);
        ServiceCategory c = named(QStringLiteral("Wash"));
        QVERIFY(repo.create(c));

        VehicleRepository vehicles(*m_db);
        Vehicle v;
        v.name = QStringLiteral("Golf");
        QVERIFY(vehicles.create(v));
        ServiceRepository services(*m_db);
        ServiceEntry e;
        e.vehicleId = v.id;
        e.categoryId = c.id;
        e.date = QDate(2026, 10, 1);
        e.description = QStringLiteral("Hand wash");
        QVERIFY2(services.create(e), qPrintable(services.errorString()));

        QVERIFY(!repo.remove(c.id));
        QVERIFY(repo.errorString().contains(QStringLiteral("still used")));
        QVERIFY(repo.byId(c.id));
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
};

QTEST_GUILESS_MAIN(TestCategoryRepository)
#include "test_categoryrepository.moc"

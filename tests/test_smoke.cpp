// Smoke test (role: tests).
// Proves that the test infrastructure works and the core library links
// without any GUI module.

#include <QTest>

#include "version.h"

/**
 * @brief Trivial test suite that keeps ctest running before real tests exist.
 */
class TestSmoke : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /** Core library is linkable and reports the version set in CMake. */
    void versionIsNotEmpty()
    {
        QVERIFY(!garagelog::core::version().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSmoke)
#include "test_smoke.moc"

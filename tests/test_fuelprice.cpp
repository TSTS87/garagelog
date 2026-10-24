// Tests for the fuel price arithmetic (role: tests).
// Pure functions, no database needed.

#include <QTest>

#include <limits>

#include "fuelprice.h"

using namespace garagelog::core;

/**
 * @brief Covers M06: deriving total, quantity or unit price from the other two.
 */
class TestFuelPrice : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /** 40.30 l for 72.50 EUR gives 1.799 EUR/l (1798.9... rounded). */
    void unitPriceFromTotalAndQuantity()
    {
        QCOMPARE(unitPriceMilli(7250, 40'300), std::optional<qint64>(1799));
        QCOMPARE(unitPriceMilli(0, 1000), std::optional<qint64>(0));
    }

    /** 40.30 l at 1.799 EUR/l costs 72.50 EUR (72.4997 rounded to the cent). */
    void totalFromQuantityAndUnitPrice()
    {
        QCOMPARE(totalCostFor(40'300, 1799), std::optional<qint64>(7250));
        // 32.5 kWh at 0.39 EUR/kWh
        QCOMPARE(totalCostFor(32'500, 390), std::optional<qint64>(1268));
    }

    /** 72.50 EUR at 1.799 EUR/l buys 40.300 l (40300.17 ml rounded). */
    void quantityFromTotalAndUnitPrice()
    {
        QCOMPARE(quantityFor(7250, 1799), std::optional<qint64>(40'300));
    }

    /** Division by zero and negative input give nullopt instead of a bogus value. */
    void invalidInput()
    {
        QCOMPARE(unitPriceMilli(7250, 0), std::nullopt);
        QCOMPARE(unitPriceMilli(-1, 1000), std::nullopt);
        QCOMPARE(quantityFor(7250, 0), std::nullopt);
        QCOMPARE(quantityFor(-1, 1799), std::nullopt);
        QCOMPARE(totalCostFor(-1, 1799), std::nullopt);
        QCOMPARE(totalCostFor(1000, -1), std::nullopt);
    }

    /** Products beyond 64 bits give nullopt instead of wrapping to a negative price. */
    void overflowIsRejected()
    {
        QCOMPARE(unitPriceMilli(999'999'999'999'900, 10'000), std::nullopt);
        QCOMPARE(totalCostFor(10'000'000'000'000, 10'000'000'000'000), std::nullopt);
        QCOMPARE(quantityFor(std::numeric_limits<qint64>::max() / 2, 1), std::nullopt);
    }

    /** Values derived from each other roundtrip: a computed total gives back the entered unit price. */
    void roundtrip()
    {
        const qint64 total = *totalCostFor(45'210, 1859);
        QCOMPARE(unitPriceMilli(total, 45'210), std::optional<qint64>(1859));
    }
};

QTEST_GUILESS_MAIN(TestFuelPrice)
#include "test_fuelprice.moc"

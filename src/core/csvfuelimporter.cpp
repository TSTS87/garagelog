// Implementation of the CSV fuel import (role: core).

#include "csvfuelimporter.h"

#include "csv.h"
#include "database.h"
#include "fuelprice.h"
#include "fuelrepository.h"
#include "valuelimits.h"

#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>

#include <algorithm>
#include <cmath>

namespace garagelog::core {

namespace {

/** true for a row that ends the data block: empty, or the start of a Fuelio "## Section". */
bool endsData(const QStringList &row)
{
    if (row.isEmpty() || (row.size() == 1 && row.first().trimmed().isEmpty())) {
        return true;
    }
    return row.first().trimmed().startsWith(QStringLiteral("##"));
}

/**
 * Parses "1.234,56" / "1,234.56" / "40.5 l" with the given decimal
 * separator; the other separator, spaces and unit/currency text are
 * dropped. nullopt for text without digits or with a letter between digits.
 */
std::optional<double> parseNumber(const QString &text, QChar decimalSeparator)
{
    QString clean;
    int firstDigit = -1;
    int lastDigit = -1;
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i).isDigit()) {
            firstDigit = firstDigit < 0 ? i : firstDigit;
            lastDigit = i;
        }
    }
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c.isDigit() || c == QLatin1Char('-') || c == decimalSeparator) {
            clean += c == decimalSeparator ? QLatin1Char('.') : c;
        } else if (i > firstDigit && i < lastDigit && c.isLetter()) {
            // Units sit before or after the number; a letter inside ("1e300",
            // "12a34") would otherwise silently merge the digits around it.
            return std::nullopt;
        }
    }
    if (!clean.contains(QRegularExpression(QStringLiteral("\\d")))) {
        return std::nullopt;
    }
    bool ok = false;
    const double value = clean.toDouble(&ok);
    return ok ? std::optional<double>(value) : std::nullopt;
}

/**
 * @p value * @p factor rounded to an integer unit (ml, m, cents, milli).
 * nullopt for a missing or negative value and above @p max: llround() of a
 * result beyond 64 bits gives LLONG_MIN, so the limit is checked first.
 */
std::optional<qint64> scaled(std::optional<double> value, double factor, qint64 max)
{
    if (!value || *value < 0 || !std::isfinite(*value) || *value * factor > double(max)) {
        return std::nullopt;
    }
    // Rounded, not truncated: 40.5 * 1000 may come out as 40499.999...
    return std::llround(*value * factor);
}

/** Interprets the many ways apps write "full tank"; nullopt if unrecognised. */
std::optional<bool> parseFull(const QString &text)
{
    const QString t = text.trimmed().toLower();
    static const QStringList yes{QStringLiteral("1"), QStringLiteral("true"), QStringLiteral("yes"), QStringLiteral("y"),
                                 QStringLiteral("ja"), QStringLiteral("x"), QStringLiteral("full"), QStringLiteral("voll")};
    static const QStringList no{QStringLiteral("0"), QStringLiteral("false"), QStringLiteral("no"), QStringLiteral("n"),
                                QStringLiteral("nein"), QStringLiteral("partial"), QStringLiteral("teil")};
    if (t.isEmpty() || yes.contains(t)) {
        return true;
    }
    if (no.contains(t)) {
        return false;
    }
    return std::nullopt;
}

/** Field @p column of @p row, or an empty string if the column is unset or missing. */
QString field(const QStringList &row, int column)
{
    return column >= 0 && column < row.size() ? row.at(column).trimmed() : QString();
}

/** Index of the first header that matches one of @p names exactly, then one that starts with one; -1 if none. */
int findColumn(const QStringList &header, const QStringList &names, const QStringList &exclude = {})
{
    auto excluded = [&](const QString &h) {
        return std::any_of(exclude.cbegin(), exclude.cend(), [&](const QString &x) { return h.contains(x); });
    };
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header.at(i).trimmed().toLower();
        if (names.contains(h) && !excluded(h)) {
            return i;
        }
    }
    for (int i = 0; i < header.size(); ++i) {
        const QString h = header.at(i).trimmed().toLower();
        for (const QString &n : names) {
            if (h.startsWith(n) && !excluded(h)) {
                return i;
            }
        }
    }
    return -1;
}

} // namespace

CsvFuelImporter::CsvFuelImporter(const Database &database)
    : m_database(database)
{
}

void CsvFuelImporter::load(const QString &text)
{
    m_rows = csv::parse(text, csv::detectDelimiter(text));
}

bool CsvFuelImporter::loadFile(const QString &path)
{
    m_error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Cannot read %1: %2").arg(path, file.errorString());
        return false;
    }
    load(QString::fromUtf8(file.readAll()));
    return true;
}

QList<QStringList> CsvFuelImporter::rows() const
{
    return m_rows;
}

QStringList CsvFuelImporter::header(const CsvFuelMapping &mapping) const
{
    return mapping.headerRow >= 0 && mapping.headerRow < m_rows.size() ? m_rows.at(mapping.headerRow) : QStringList();
}

CsvFuelMapping CsvFuelImporter::suggestMapping() const
{
    CsvFuelMapping m;
    for (int r = 0; r < m_rows.size(); ++r) {
        if (!m_rows.at(r).isEmpty() && m_rows.at(r).first().trimmed().compare(QStringLiteral("## Log"), Qt::CaseInsensitive) == 0) {
            m.headerRow = r + 1;
            break;
        }
    }
    const QStringList h = header(m);

    // Fuelio's names first ("Data" is its date column), then common
    // English/German spellings. "/100" excludes consumption columns like
    // "l/100km" that would otherwise match "km".
    m.dateColumn = findColumn(h, {QStringLiteral("data"), QStringLiteral("date"), QStringLiteral("datum")});
    m.odometerColumn = findColumn(h, {QStringLiteral("odo (km)"), QStringLiteral("odometer"), QStringLiteral("odo"), QStringLiteral("mileage"),
                                      QStringLiteral("kilometerstand"), QStringLiteral("tachostand"), QStringLiteral("km")},
                                  {QStringLiteral("/100")});
    m.quantityColumn = findColumn(h, {QStringLiteral("fuel (litres)"), QStringLiteral("litres"), QStringLiteral("liters"), QStringLiteral("liter"),
                                      QStringLiteral("quantity"), QStringLiteral("volume"), QStringLiteral("menge"), QStringLiteral("kwh")},
                                  {QStringLiteral("price"), QStringLiteral("/100"), QStringLiteral("preis")});
    m.totalCostColumn = findColumn(h, {QStringLiteral("price (optional)"), QStringLiteral("total"), QStringLiteral("total cost"), QStringLiteral("cost"),
                                       QStringLiteral("betrag"), QStringLiteral("gesamtpreis"), QStringLiteral("kosten"), QStringLiteral("price")},
                                   {QStringLiteral("volume"), QStringLiteral("/100"), QStringLiteral("unit"), QStringLiteral("per")});
    m.unitPriceColumn = findColumn(h, {QStringLiteral("volumeprice"), QStringLiteral("unit price"), QStringLiteral("price per"), QStringLiteral("preis pro"),
                                       QStringLiteral("literpreis")});
    m.fullTankColumn = findColumn(h, {QStringLiteral("full"), QStringLiteral("full tank"), QStringLiteral("volltank"), QStringLiteral("voll")});
    m.stationColumn = findColumn(h, {QStringLiteral("station"), QStringLiteral("tankstelle"), QStringLiteral("city (optional)"), QStringLiteral("city")});
    m.notesColumn = findColumn(h, {QStringLiteral("notes (optional)"), QStringLiteral("notes"), QStringLiteral("note"), QStringLiteral("notiz"),
                                   QStringLiteral("bemerkung")});

    // A comma in a number column without a dot means German style "40,5".
    for (int r = m.headerRow + 1; r < m_rows.size() && r <= m.headerRow + 10 && !endsData(m_rows.at(r)); ++r) {
        const QString q = field(m_rows.at(r), m.quantityColumn);
        if (q.contains(QLatin1Char(',')) && !q.contains(QLatin1Char('.'))) {
            m.decimalSeparator = QLatin1Char(',');
            break;
        }
    }
    // Guess the date format from the first data row.
    if (m.headerRow + 1 < m_rows.size()) {
        const QString d = field(m_rows.at(m.headerRow + 1), m.dateColumn);
        for (const QString &format : {QStringLiteral("yyyy-MM-dd"), QStringLiteral("dd.MM.yyyy"), QStringLiteral("dd/MM/yyyy"),
                                      QStringLiteral("MM/dd/yyyy"), QStringLiteral("yyyy-MM-dd HH:mm"), QStringLiteral("yyyy-MM-dd HH:mm:ss")}) {
            if (QDateTime::fromString(d, format).isValid() || QDate::fromString(d, format).isValid()) {
                m.dateFormat = format;
                break;
            }
        }
    }
    return m;
}

CsvFuelPreview CsvFuelImporter::preview(const CsvFuelMapping &m) const
{
    CsvFuelPreview result;
    if (m.dateColumn < 0 || m.odometerColumn < 0 || m.quantityColumn < 0 || (m.totalCostColumn < 0 && m.unitPriceColumn < 0)) {
        result.errors.append(QStringLiteral("Please assign date, odometer, quantity and total price or unit price"));
        return result;
    }

    // headerRow is -1 without a header line; anything lower is clamped.
    for (int r = std::max(0, m.headerRow + 1); r < m_rows.size(); ++r) {
        const QStringList &row = m_rows.at(r);
        if (endsData(row)) {
            break;
        }
        const int line = r + 1;
        auto reject = [&](const QString &what, const QString &value) {
            result.errors.append(QStringLiteral("Row %1: invalid %2 \"%3\"").arg(line).arg(what, value));
        };

        // Date with time (Fuelio can export "yyyy-MM-dd HH:mm") keeps only the day.
        const QString dateText = field(row, m.dateColumn);
        QDate date = QDate::fromString(dateText, m.dateFormat);
        if (!date.isValid()) {
            date = QDateTime::fromString(dateText, m.dateFormat).date();
        }
        if (!limits::isStorableDate(date)) {
            reject(QStringLiteral("date"), dateText);
            continue;
        }
        const auto odometerM = scaled(parseNumber(field(row, m.odometerColumn), m.decimalSeparator), 1000.0, limits::maxDistanceM);
        if (!odometerM) {
            reject(QStringLiteral("odometer"), field(row, m.odometerColumn));
            continue;
        }
        const auto quantity = scaled(parseNumber(field(row, m.quantityColumn), m.decimalSeparator), 1000.0, limits::maxQuantity);
        if (!quantity || *quantity <= 0) {
            reject(QStringLiteral("quantity"), field(row, m.quantityColumn));
            continue;
        }
        const auto full = m.fullTankColumn < 0 ? std::optional<bool>(true) : parseFull(field(row, m.fullTankColumn));
        if (!full) {
            reject(QStringLiteral("full tank value"), field(row, m.fullTankColumn));
            continue;
        }

        FuelEntry e;
        e.date = date;
        e.odometerM = *odometerM;
        e.quantity = *quantity;
        e.fullTank = *full;
        e.station = field(row, m.stationColumn);
        e.notes = field(row, m.notesColumn);

        const auto total = scaled(parseNumber(field(row, m.totalCostColumn), m.decimalSeparator), 100.0, limits::maxMoneyC);
        if (total && *total > 0) {
            e.totalCostC = *total;
        } else {
            const auto unit = scaled(parseNumber(field(row, m.unitPriceColumn), m.decimalSeparator), 1000.0, limits::maxMoneyC * 10);
            const auto calculated = unit ? totalCostFor(e.quantity, *unit) : std::nullopt;
            if (!calculated || *calculated > limits::maxMoneyC) {
                reject(QStringLiteral("price"), field(row, m.totalCostColumn) + field(row, m.unitPriceColumn));
                continue;
            }
            e.totalCostC = *calculated;
        }
        result.entries.append(e);
        result.lines.append(line);
    }
    return result;
}

int CsvFuelImporter::importInto(const QString &vehicleId, const CsvFuelMapping &mapping)
{
    m_error.clear();
    const CsvFuelPreview p = preview(mapping);
    QSqlDatabase db = m_database.connection();
    if (!db.transaction()) {
        m_error = QStringLiteral("Cannot start transaction: %1").arg(db.lastError().text());
        return -1;
    }
    FuelRepository repo(m_database);
    for (FuelEntry e : p.entries) {
        e.vehicleId = vehicleId;
        if (!repo.create(e)) {
            db.rollback();
            m_error = repo.errorString();
            return -1;
        }
    }
    if (!db.commit()) {
        m_error = QStringLiteral("Cannot commit import: %1").arg(db.lastError().text());
        db.rollback();
        return -1;
    }
    return static_cast<int>(p.entries.size());
}

QString CsvFuelImporter::errorString() const
{
    return m_error;
}

} // namespace garagelog::core

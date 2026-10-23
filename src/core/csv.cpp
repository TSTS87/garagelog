// Implementation of the CSV helpers (role: core).

#include "csv.h"

#include <array>

namespace garagelog::core::csv {

QList<QStringList> parse(const QString &text, QChar delimiter)
{
    QList<QStringList> rows;
    QStringList fields;
    QString field;
    bool inQuotes = false;
    // true once the current row has any content, so a trailing line break
    // does not add an empty row.
    bool rowStarted = false;

    qsizetype i = text.startsWith(QChar(0xFEFF)) ? 1 : 0;
    for (; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                    field += QLatin1Char('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
            continue;
        }
        if (c == QLatin1Char('"')) {
            inQuotes = true;
            rowStarted = true;
        } else if (c == delimiter) {
            fields.append(field);
            field.clear();
            rowStarted = true;
        } else if (c == QLatin1Char('\r') || c == QLatin1Char('\n')) {
            if (c == QLatin1Char('\r') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n')) {
                ++i;
            }
            if (rowStarted || !field.isEmpty()) {
                fields.append(field);
                rows.append(fields);
            } else {
                rows.append(QStringList()); // keep empty lines: they separate Fuelio sections
            }
            fields.clear();
            field.clear();
            rowStarted = false;
        } else {
            field += c;
            rowStarted = true;
        }
    }
    if (rowStarted || !field.isEmpty()) {
        fields.append(field);
        rows.append(fields);
    }
    return rows;
}

QChar detectDelimiter(const QString &text)
{
    const std::array<QChar, 3> candidates{QLatin1Char(','), QLatin1Char(';'), QLatin1Char('\t')};
    std::array<int, 3> counts{};
    bool inQuotes = false;
    int lines = 0;
    for (const QChar c : text) {
        if (c == QLatin1Char('"')) {
            inQuotes = !inQuotes;
        } else if (!inQuotes && c == QLatin1Char('\n') && ++lines >= 20) {
            break;
        } else if (!inQuotes) {
            for (std::size_t k = 0; k < candidates.size(); ++k) {
                counts[k] += c == candidates[k] ? 1 : 0;
            }
        }
    }
    std::size_t best = 0;
    for (std::size_t k = 1; k < candidates.size(); ++k) {
        if (counts[k] > counts[best]) {
            best = k;
        }
    }
    return candidates[best];
}

QString escape(const QString &field, QChar delimiter)
{
    const bool needsQuotes = field.contains(delimiter) || field.contains(QLatin1Char('"'))
        || field.contains(QLatin1Char('\n')) || field.contains(QLatin1Char('\r'));
    if (!needsQuotes) {
        return field;
    }
    QString quoted = field;
    quoted.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

QString row(const QStringList &fields, QChar delimiter)
{
    QStringList escaped;
    escaped.reserve(fields.size());
    for (const QString &f : fields) {
        escaped.append(escape(f, delimiter));
    }
    return escaped.join(delimiter) + QStringLiteral("\r\n");
}

} // namespace garagelog::core::csv

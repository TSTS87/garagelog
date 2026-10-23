// Generator for large synthetic garages (role: tools, developer-only).
// Fills a garage with one test vehicle and thousands of plausible fuel and
// service entries, for the M15 performance test and for looking at a "full"
// app. Not part of the shipped program; links only garagelog_core.

#pragma once

#include <QDate>
#include <QString>

namespace garagelog::core {
class Database;
}

namespace garagelog::tools {

/** What generate() created, for reporting and assertions. */
struct SampleDataResult {
    QString vehicleId;     ///< id of the created test vehicle
    int fuelEntries = 0;   ///< number of fuel entries written
    int serviceEntries = 0;///< number of service entries written
    int intervals = 0;     ///< number of maintenance intervals written
};

/**
 * @brief Adds one test car with @p fuelCount daily fuel entries ending @p lastDay.
 *
 * Entries are one day apart and 350–650 km each, about 10 % are partial
 * fills; prices drift slowly so the charts have a shape. Every 15,000 km
 * an oil change and every year insurance and tax are added as service
 * entries, plus three intervals. The data is pseudo-random with a fixed
 * seed, so two runs produce the same values (ids and timestamps differ).
 * Everything is written in one transaction: on error nothing is kept.
 *
 * @param database open garage; existing data is left untouched
 * @param fuelCount number of fuel entries, > 0
 * @param lastDay date of the newest fuel entry, usually today
 * @param error receives the reason if generation fails, may be nullptr
 * @return the counts, or a result with empty vehicleId on failure
 */
SampleDataResult generateSampleData(const core::Database &database, int fuelCount, const QDate &lastDay,
                                    QString *error = nullptr);

} // namespace garagelog::tools

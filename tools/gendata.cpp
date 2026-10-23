// Command line tool garagelog-gendata (role: tools, developer-only).
// Creates or extends a garage file with a test car and many fuel entries,
// for the M15 performance check and for screenshots of a "full" app.
// Usage: garagelog-gendata <garage.sqlite> [fuel entry count, default 10000]

#include "database.h"
#include "sampledata.h"

#include <QCoreApplication>
#include <QDate>
#include <QElapsedTimer>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    bool countOk = true;
    const int count = args.size() > 2 ? args.at(2).toInt(&countOk) : 10000;
    if (args.size() < 2 || args.size() > 3 || !countOk || count <= 0) {
        err << "Usage: garagelog-gendata <garage.sqlite> [fuel entry count, default 10000]\n";
        return 2;
    }

    // Database::open() creates the file and runs the migrations if needed.
    garagelog::core::Database database;
    if (!database.open(args.at(1))) {
        err << "Cannot open garage: " << database.errorString() << '\n';
        return 1;
    }

    QElapsedTimer timer;
    timer.start();
    QString error;
    const auto result = garagelog::tools::generateSampleData(database, count, QDate::currentDate(), &error);
    if (result.vehicleId.isEmpty()) {
        err << "Generation failed: " << error << '\n';
        return 1;
    }
    out << "Added 1 vehicle, " << result.fuelEntries << " fuel entries, " << result.serviceEntries
        << " service entries and " << result.intervals << " intervals to " << database.filePath() << " in "
        << timer.elapsed() << " ms\n";
    return 0;
}

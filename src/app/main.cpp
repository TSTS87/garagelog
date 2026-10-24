// Entry point of the GarageLog executable (role: app).
// Sets application metadata, opens the last used garage database, registers the
// models as QML singletons, creates the QML engine and loads qml/Main.qml from
// the embedded resources. Exits with an error code if the database or QML
// fails to load.

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QUrl>

#include <KConfigGroup>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KSharedConfig>

#include "database.h"
#include "datatransfer.h"
#include "format.h"
#include "fuellistmodel.h"
#include "garagecontroller.h"
#include "intervallistmodel.h"
#include "servicelistmodel.h"
#include "statsmodel.h"
#include "vehiclelistmodel.h"
#include "version.h"

// Start time measurement for the M15 performance check; off by default, enable
// with QT_LOGGING_RULES="garagelog.startup.debug=true".
Q_LOGGING_CATEGORY(lcStartup, "garagelog.startup", QtWarningMsg)

int main(int argc, char *argv[])
{
    QElapsedTimer startTimer;
    startTimer.start();
    QGuiApplication app(argc, argv);
    // Translation catalogue name (garagelog.mo); must be set before any i18n() call.
    KLocalizedString::setApplicationDomain("garagelog");

    // Organization/app names determine QStandardPaths locations (database path from M02 on).
    QCoreApplication::setOrganizationDomain(QStringLiteral("tmx.org"));
    QCoreApplication::setApplicationName(QStringLiteral("garagelog"));
    QCoreApplication::setApplicationVersion(garagelog::core::version());
    // Must match the .desktop file name so Wayland compositors show the right icon.
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.TSTS87.GarageLog"));
    // X11 and the window list need the icon explicitly; it comes from the installed hicolor theme.
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.TSTS87.GarageLog")));

    // Only --help and --version for now; lets bug reports name the exact version.
    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("Track fuel, maintenance and costs of your vehicles"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    // The garage chosen last on the settings page is remembered in
    // ~/.config/garagelogrc (KConfig), outside the garage file itself.
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("General"));
    const QString defaultPath = garagelog::core::Database::defaultFilePath();
    const QString lastPath = config.readEntry("GaragePath", defaultPath);

    // Declared before the engine so it outlives everything QML may later use.
    // A remembered garage that is gone (USB stick, renamed) is not created
    // again empty at the old place: the default garage opens instead and the
    // user is told. Without any usable database the app cannot do anything.
    garagelog::core::Database database;
    using OpenMode = garagelog::core::Database::OpenMode;
    QString startupNotice;
    if (!database.open(lastPath, lastPath == defaultPath ? OpenMode::CreateIfMissing : OpenMode::MustExist)) {
        // Only the file name: logs end up in bug reports, home paths should not.
        qWarning().noquote() << "Cannot open remembered garage" << QFileInfo(lastPath).fileName();
        if (lastPath == defaultPath || !database.open(defaultPath)) {
            qCritical().noquote() << "Cannot open garage database:" << database.errorString();
            return 1;
        }
        startupNotice = i18n("The garage %1 could not be opened, the default garage is open instead. Use “Open Garage” in the settings to choose it again.", lastPath);
    }

    // The models live on main()'s stack, after the database and before the engine,
    // so QML never sees them destroyed.
    garagelog::app::VehicleListModel vehicles(database);
    garagelog::app::FuelListModel fuelEntries(database);
    garagelog::app::ServiceListModel serviceEntries(database);
    using garagelog::app::IntervalListModel;
    IntervalListModel intervals(database, IntervalListModel::Scope::Vehicle);
    IntervalListModel dueIntervals(database, IntervalListModel::Scope::AllVehicles);
    garagelog::app::StatsModel stats(database);
    garagelog::app::Format format;
    garagelog::app::GarageController garage(database, format);
    garage.setStartupNotice(startupNotice);
    garagelog::app::DataTransfer dataTransfer(database);
    // A saved fuel entry can raise the odometer shown in the drawer and the
    // page header; an edited vehicle can switch the fuel unit (l <-> kWh).
    QObject::connect(&fuelEntries, &garagelog::app::FuelListModel::entriesChanged, &vehicles, &garagelog::app::VehicleListModel::reload);
    QObject::connect(&vehicles, &garagelog::app::VehicleListModel::reloaded, &fuelEntries, &garagelog::app::FuelListModel::reload);
    // Service entries can carry an odometer reading as well.
    QObject::connect(&serviceEntries, &garagelog::app::ServiceListModel::entriesChanged, &vehicles, &garagelog::app::VehicleListModel::reload);
    // Every fuel/service change and vehicle edit ends in Vehicles.reloaded,
    // so the due status follows new odometers, services and archiving.
    QObject::connect(&vehicles, &garagelog::app::VehicleListModel::reloaded, &intervals, &IntervalListModel::reload);
    QObject::connect(&vehicles, &garagelog::app::VehicleListModel::reloaded, &dueIntervals, &IntervalListModel::reload);
    QObject::connect(&vehicles, &garagelog::app::VehicleListModel::reloaded, &stats, &garagelog::app::StatsModel::reload);
    QObject::connect(&intervals, &IntervalListModel::intervalsChanged, &dueIntervals, &IntervalListModel::reload);
    QObject::connect(&dueIntervals, &IntervalListModel::intervalsChanged, &intervals, &IntervalListModel::reload);
    // After switching the garage or importing data, or when the units change,
    // everything reloads: Vehicles.reloaded cascades to fuel, intervals and
    // stats; ServiceEntries has no such link and is reloaded directly.
    QObject::connect(&garage, &garagelog::app::GarageController::garageChanged, &vehicles, &garagelog::app::VehicleListModel::reload);
    QObject::connect(&garage, &garagelog::app::GarageController::garageChanged, &serviceEntries, &garagelog::app::ServiceListModel::reload);
    QObject::connect(&dataTransfer, &garagelog::app::DataTransfer::dataImported, &vehicles, &garagelog::app::VehicleListModel::reload);
    QObject::connect(&dataTransfer, &garagelog::app::DataTransfer::dataImported, &serviceEntries, &garagelog::app::ServiceListModel::reload);
    QObject::connect(&format, &garagelog::app::Format::unitsChanged, &vehicles, &garagelog::app::VehicleListModel::reload);
    QObject::connect(&format, &garagelog::app::Format::unitsChanged, &serviceEntries, &garagelog::app::ServiceListModel::reload);
    QObject::connect(&garage, &garagelog::app::GarageController::garageChanged, [&config, &database] {
        config.writeEntry("GaragePath", database.filePath());
        config.sync();
    });

    // Singleton instances: QML uses them as Vehicles.xxx / Format.xxx after
    // "import org.tmx.garagelog", without creating its own copies.
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "Vehicles", &vehicles);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "FuelEntries", &fuelEntries);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "ServiceEntries", &serviceEntries);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "Intervals", &intervals);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "DueIntervals", &dueIntervals);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "Stats", &stats);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "Format", &format);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "Garage", &garage);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "DataTransfer", &dataTransfer);
    // Design tokens of the own look (ADR-015) live in QML, not C++.
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/qml/Style.qml")), "org.tmx.garagelog", 1, 0, "Style");

    QQmlApplicationEngine engine;
    // Makes i18n()/i18nc() available in all QML files of this engine.
    KLocalization::setupLocalizedContext(&engine);
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));

    // A QML error leaves no root object; quit instead of running without a window.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.load(url);

    // The first rendered frame is what the user perceives as "started".
    if (!engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
            QObject::connect(window, &QQuickWindow::frameSwapped, &app, [&startTimer] {
                qCDebug(lcStartup) << "first frame after" << startTimer.elapsed() << "ms";
            }, Qt::SingleShotConnection);
        }
    }
    return app.exec();
}

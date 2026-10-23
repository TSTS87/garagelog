// Entry point of the GarageLog executable (role: app).
// Sets application metadata, opens the default garage database, registers the
// models as QML singletons, creates the QML engine and loads qml/Main.qml from
// the embedded resources. Exits with an error code if the database or QML
// fails to load.

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QUrl>

#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include "database.h"
#include "format.h"
#include "fuellistmodel.h"
#include "servicelistmodel.h"
#include "vehiclelistmodel.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Translation catalogue name (garagelog.mo); must be set before any i18n() call.
    KLocalizedString::setApplicationDomain("garagelog");

    // Organization/app names determine QStandardPaths locations (database path from M02 on).
    QCoreApplication::setOrganizationDomain(QStringLiteral("tmx.org"));
    QCoreApplication::setApplicationName(QStringLiteral("garagelog"));
    QCoreApplication::setApplicationVersion(garagelog::core::version());
    // Must match the .desktop file name so Wayland compositors show the right icon.
    QGuiApplication::setDesktopFileName(QStringLiteral("org.tmx.GarageLog"));

    // Declared before the engine so it outlives everything QML may later use.
    // Without a usable database the app cannot do anything, so fail early;
    // a user-facing error dialog comes with the garage file handling in the UI.
    garagelog::core::Database database;
    if (!database.open(garagelog::core::Database::defaultFilePath())) {
        qCritical().noquote() << "Cannot open garage database:" << database.errorString();
        return 1;
    }

    // The models live on main()'s stack, after the database and before the engine,
    // so QML never sees them destroyed.
    garagelog::app::VehicleListModel vehicles(database);
    garagelog::app::FuelListModel fuelEntries(database);
    garagelog::app::ServiceListModel serviceEntries(database);
    garagelog::app::Format format;
    // A saved fuel entry can raise the odometer shown in the drawer and the
    // page header; an edited vehicle can switch the fuel unit (l <-> kWh).
    QObject::connect(&fuelEntries, &garagelog::app::FuelListModel::entriesChanged, &vehicles, &garagelog::app::VehicleListModel::reload);
    QObject::connect(&vehicles, &garagelog::app::VehicleListModel::reloaded, &fuelEntries, &garagelog::app::FuelListModel::reload);
    // Service entries can carry an odometer reading as well.
    QObject::connect(&serviceEntries, &garagelog::app::ServiceListModel::entriesChanged, &vehicles, &garagelog::app::VehicleListModel::reload);
    // Singleton instances: QML uses them as Vehicles.xxx / Format.xxx after
    // "import org.tmx.garagelog", without creating its own copies.
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "Vehicles", &vehicles);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "FuelEntries", &fuelEntries);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "ServiceEntries", &serviceEntries);
    qmlRegisterSingletonInstance("org.tmx.garagelog", 1, 0, "Format", &format);

    QQmlApplicationEngine engine;
    // Makes i18n()/i18nc() available in all QML files of this engine.
    KLocalization::setupLocalizedContext(&engine);
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));

    // A QML error leaves no root object; quit instead of running without a window.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}

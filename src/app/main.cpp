// Entry point of the GarageLog executable (role: app).
// Sets application metadata, opens the default garage database, creates the
// QML engine and loads qml/Main.qml from the embedded resources. Exits with an
// error code if the database or QML fails to load.

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

#include "database.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

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

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));

    // A QML error leaves no root object; quit instead of running without a window.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}

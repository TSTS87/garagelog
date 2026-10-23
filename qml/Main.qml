// Root QML file of GarageLog (role: qml, pure UI).
// Defines the main window. In M01 it is intentionally empty; drawer and pages
// follow in later milestones (see docs/SPEC.md §6).

import QtQuick
import org.kde.kirigami as Kirigami

// Kirigami.ApplicationWindow is the Kirigami top-level window: it brings a
// pageStack (PageRow) plus slots for globalDrawer/contextDrawer, which the
// SPEC layout (drawer with vehicles + pages) builds on later.
Kirigami.ApplicationWindow {
    id: root

    title: "GarageLog"
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 35
}

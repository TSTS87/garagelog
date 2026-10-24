// Design tokens of GarageLog's own look (role: qml, pure UI, ADR-015).
// Accent palette per vehicle type, corner radii and the scale of the large
// figures. Window, text and status colours stay with Kirigami.Theme; only
// content surfaces (hero, icon tiles, bars) use the fixed colours from here.

// "pragma Singleton" makes this file one shared object; main.cpp registers
// it as Style in org.tmx.garagelog, so every file reads the same tokens.
pragma Singleton

import QtQuick
import org.kde.kirigami as Kirigami

QtObject {
    /** Corner radius of cards and the hero; rounder than Kirigami's default for a softer look. */
    readonly property real cardRadius: Kirigami.Units.gridUnit * 0.8
    /** Corner radius of icon tiles and list rows inside cards. */
    readonly property real tileRadius: Kirigami.Units.gridUnit * 0.5

    /** Font size factor of hero figures (odometer) relative to the default font. */
    readonly property real heroFigureScale: 2.6
    /** Font size factor of key figures in stat tiles. */
    readonly property real figureScale: 1.75

    /**
     * Gradient pair [light, deep] per vehicle type (database text). White
     * text stays readable on both ends, in light and dark colour schemes.
     */
    readonly property var typePalette: ({
        car: ["#3b82f6", "#1e3a8a"],
        motorcycle: ["#f97316", "#9a3412"],
        scooter: ["#14b8a6", "#115e59"],
        ebike: ["#8b5cf6", "#4c1d95"],
        other: ["#64748b", "#1e293b"]
    })

    /** Colours for chart series and category bars, starting with the brand blue. */
    readonly property var seriesColors: ["#3b82f6", "#f97316", "#14b8a6", "#8b5cf6", "#ec4899", "#eab308", "#64748b"]

    /** Main accent colour of a vehicle type; unknown types get the neutral slate. */
    function accent(type) {
        return (typePalette[type] ?? typePalette.other)[0];
    }

    /** Darker end of the type gradient, used for the hero's right side. */
    function accentDeep(type) {
        return (typePalette[type] ?? typePalette.other)[1];
    }
}

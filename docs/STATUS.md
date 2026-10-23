# STATUS – Übergabedatei zwischen Sessions

**Diese Datei ist die einzige Wahrheit über den aktuellen Stand.** Claude Code liest sie als Erstes und schreibt sie als Letztes. Nach `/clear` geht nur über diese Datei die Arbeit richtig weiter.

Regeln:
- Nur Fakten, die aus Code und Git ableitbar sind. Nichts „geplant", nichts „sollte".
- Kurz halten (< 60 Zeilen). Alte Meilensteine nur als eine Zeile in der Historie.
- Pflichtfelder immer ausfüllen, auch wenn „keine".

---

## Aktueller Meilenstein
M02 – Datenbank & Migration

## Zustand
- Letzter Commit: `1471f96` docs: fix Arch package list in build instructions
- Build grün: ja (`cmake --build build`, Qt 6.12, KF 6.31, cmake 4.4)
- Tests grün: ja (2/2: `test_smoke`, `appstreamtest` – letzterer kommt automatisch von ECM und validiert metainfo.xml)
- App startet: ja (offscreen 3 s ohne QML-Fehler geprüft; leeres Kirigami-Fenster „GarageLog")

## Offene Punkte im aktuellen Meilenstein
- alle (noch nicht begonnen)

## Verschoben / Abweichungen vom Plan
- keine

## Bekannte Probleme
- `git push` ist in `.claude/settings.json` gesperrt → Thomas pusht selbst (VS Code „Synchronisieren" oder `git push`)

## Hinweise für die nächste Session
- `main.cpp` nutzt `QGuiApplication`; für den Plasma-Look (Style `org.kde.desktop`) wäre `QApplication` + Qt6::Widgets nötig → nur mit ADR entscheiden
- QML wird per `qt_add_resources` als `qrc:/qml/Main.qml` eingebettet (src/app/CMakeLists.txt); neue QML-Dateien dort eintragen
- Core-Lib: `garagelog_core` (src/core), Include-Pfad public, Namespace `garagelog::core`
- App-ID im Manifest ist Platzhalter `org.tmx.GarageLog` – Thomas entscheidet (siehe Kommentar in `packaging/flatpak/*.yml`)

## Historie (eine Zeile pro abgeschlossenem Meilenstein)
- M01 Projektgerüst – 2026-10-23 – `1471f96`

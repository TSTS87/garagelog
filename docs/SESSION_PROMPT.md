# Session-Prompts

## 1. Allererste Session (nur einmal)

```
Lies CLAUDE.md, docs/SPEC.md, docs/DATA_MODEL.md, docs/DECISIONS.md, docs/ROADMAP.md und docs/STATUS.md vollständig. Fasse mir in maximal 8 Sätzen auf Deutsch zusammen, was die App ist, was Phase 1 umfasst und was ausdrücklich nicht gebaut wird.

Prüfe dann mit EINEM Befehl, ob Qt 6, Kirigami und ECM installiert sind. Warte auf meine Ausgabe. Fehlt etwas: genau ein pacman-Befehl, dann wieder warten.

Danach `git init`, erster Commit des Gerüsts, und Start von M01 nach dem Ablauf in CLAUDE.md → Abschnitt „Ablauf einer Session".
```

## 2. Jede weitere Session (Standard – nach `/clear` einfügen)

```
Lies CLAUDE.md und docs/STATUS.md. Bestätige mir in 2 Sätzen: welcher Meilenstein ist aktuell, und was ist der erste offene Punkt. Prüfe mit `git status` und `git log --oneline -5`, dass STATUS.md zum Repo passt – bei Widerspruch: anhalten und mich fragen. Dann arbeite nach CLAUDE.md → „Ablauf einer Session".
```

Das ist alles. Spec, Datenmodell und Roadmap liest Claude Code selbst nach, wenn der Meilenstein es verlangt (steht in CLAUDE.md).

## 3. Vor dem Release (M18)

```
Lies docs/SPEC.md Abschnitt 7. Prüfe jedes Kriterium im Code bzw. durch Ausführen und gib mir eine ehrliche Tabelle: erfüllt / nicht erfüllt / nicht prüfbar, mit Beleg. Keine Beschönigung.
```

## Wenn etwas schiefgeht

Build kaputt nach `/clear` und STATUS.md sagt „grün":
```
STATUS.md behauptet Build grün, Build schlägt aber fehl. Finde heraus, ob das an meiner Umgebung liegt oder am Code. Ein Befehl nach dem anderen.
```

Claude Code will springen (Features aus späteren Meilensteinen):
```
Stopp. Das ist nicht im aktuellen Meilenstein. Zurück zu STATUS.md.
```

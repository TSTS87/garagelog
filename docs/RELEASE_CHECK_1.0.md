# Release check 1.0.0 (SPEC §7)

Checked on 2026-10-24 against the release commit tagged `v1.0.0`; `v1.0.1` only replaces the screenshots.
Machine: Intel Core i7-11800H (2021), CachyOS, Qt 6.12, KDE Frameworks 6.31; Flatpak on org.kde.Platform 6.11.
Legend: **pass** = verified with the evidence given · **open** = needs a manual check by TSTS87 on a real desktop.

| # | Criterion | Result | Evidence |
|---|---|---|---|
| 1 | All calculations (consumption, due dates, cost per km) unit-tested | **pass** | `test_fuelstats` (consumption between full tanks, partial fills), `test_fuelprice` (two-of-three price input), `test_duecalculator` (distance/time/combined, overdue, unknown), `test_statsservice` (cost per km, per month, average consumption, categories), plus model tests `test_fuellistmodel`, `test_statsmodel`, `test_intervallistmodel`. 26/26 tests green locally and in CI. |
| 2 | Flatpak builds from the repository with `flatpak-builder` | **pass** | `flatpak run org.flatpak.Builder --user --install --force-clean build-flatpak packaging/flatpak/io.github.TSTS87.GarageLog.yml` succeeds; the app starts from the Flatpak (first frame after 357 ms). CI job `flatpak` builds the same manifest on every push. |
| 3 | New vehicle + first fuel entry in < 60 s without documentation | **open** | Not measurable by an automated check. Path: empty garage → placeholder button "Add Vehicle" → form (name is the only required field) → "Add Entry" → date prefilled, odometer, quantity, total price → Save. All forms work with keyboard only (M15). TSTS87: time one run with a person who has not used the app. |
| 4 | Start < 1 s on 5-year-old hardware | **pass** (offscreen) | `QT_LOGGING_RULES="garagelog.startup.debug=true"`: empty garage 154–243 ms to the first frame (3 runs), Flatpak 357 ms, 10,000 entries about 300 ms (M15). The test machine's CPU is from 2021. Not yet timed on a real Plasma session. |
| 5 | No crashes with an empty database; smooth with 10,000 entries | **pass** / **open** | Empty garage: starts and shows the empty states (M15), all 26 tests create empty garages. 10,000 entries: `test_performance` loads all models in 202 ms, reloads the fuel list in 35 ms. "Smooth scrolling" is a visual check → TSTS87 with `~/GarageLog-Testgarage/garage.sqlite`. |
| 6 | README with screenshots, AppStream metadata complete | **pass** | README shows three screenshots (real KWin window captures since 1.0.1). `appstreamcli validate` passes with network checks after the repository went public (only the pedantic note about upper-case letters in the `io.github.TSTS87` id); `desktop-file-validate` passes; both run as CTest. Flathub manifest builds v1.0.1 from GitHub; `flatpak-builder-lint manifest` is clean. |

## Known limitations shipped in 1.0.0
- Error messages from the core library (validation, database errors) are English only.
- Breeze has no motorcycle/scooter icon; all vehicle types show the car icon.
- Dates are typed into a text field (short locale format or ISO); there is no date picker.
- In the Flatpak, garages outside `~/Documents` cannot use attachments without an extra permission (ADR-014).

## Manual checks still to do (TSTS87, real Plasma desktop)
- Criteria 3 and 5 (visual smoothness) above
- Native file and folder dialogs, natively and in the Flatpak (portal)
- Overall look on Plasma (all screens since M04)

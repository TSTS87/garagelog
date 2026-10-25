# GarageLog statistics

Weekly snapshots written by the Stats workflow (`.github/workflows/stats.yml` on main, ADR-017).
This branch has no code and no shared history with main.

- `downloads.csv` – total downloads per release file at the date of the snapshot
- `repository.csv` – stars, forks, watchers, open issues
- `views.csv`, `clones.csv` – per day (GitHub keeps only 14 days, so the weekly run preserves them)
- `referrers.csv` – top referring sites, 14-day totals at the date of the snapshot

# Flathub submission

`io.github.TSTS87.GarageLog.yml` is the manifest for Flathub. Unlike the development manifest in
`packaging/flatpak/` it builds the release tag from GitHub, pinned to the tag's commit.

## Before submitting
1. The GitHub repository must be public (it is since 2026-10-24) – Flathub fetches the sources,
   the AppStream screenshots and checks the homepage.
2. Lint the manifest and a built repository (needs `org.flatpak.Builder` from Flathub):
   ```
   flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest packaging/flathub/io.github.TSTS87.GarageLog.yml
   flatpak run org.flatpak.Builder --user --force-clean --repo=repo-flatpak build-flatpak packaging/flathub/io.github.TSTS87.GarageLog.yml
   flatpak run --command=flatpak-builder-lint org.flatpak.Builder repo repo-flatpak
   ```
   If `bwrap` complains about `/run/user/1000/doc`, the document portal is not running:
   add `--no-documents-portal` after `flatpak run`.
   Expected result (checked for v1.1.0): the manifest lint is clean; the repo lint only reports
   `appstream-external-screenshot-url` / `appstream-screenshots-not-mirrored-in-ostree` /
   `appstream-remote-icon-not-mirrored`. Flathub's own build pipeline mirrors screenshots and icons
   to dl.flathub.org, which a local build cannot fully reproduce.

## Submitting (first release)
Following <https://docs.flathub.org/docs/for-app-authors/submission>:
1. Fork <https://github.com/flathub/flathub> on GitHub (untick "copy the master branch only").
2. `git clone --branch=new-pr git@github.com:TSTS87/flathub.git && cd flathub`
3. `git checkout -b garagelog new-pr`, copy `io.github.TSTS87.GarageLog.yml` into the repository root,
   commit and push.
4. Open a pull request against the `new-pr` branch of flathub/flathub, titled
   "Add io.github.TSTS87.GarageLog", and answer the reviewers.
   Expect questions about `--filesystem=xdg-documents` – the reason is in ADR-014.
5. After the merge Flathub creates `flathub/io.github.TSTS87.GarageLog`; you get write access to it.
   Sign in to <https://flathub.org> with GitHub to verify the app (io.github ids verify via the account).

## New release
1. Add a `<release>` entry to `packaging/io.github.TSTS87.GarageLog.metainfo.xml`, bump `project(VERSION)`.
2. Tag `vX.Y.Z`, push the tag, create the GitHub release.
3. In the Flathub repository update `tag` and `commit` in the manifest (the `x-checker-data` block lets
   Flathub's external data checker open that pull request automatically).

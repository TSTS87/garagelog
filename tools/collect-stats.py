#!/usr/bin/env python3
# Collects download and traffic numbers of the GitHub repository into CSV files (role: developer tool).
# Run weekly by .github/workflows/stats.yml, which commits the CSVs to the "stats" branch.
# GitHub keeps traffic data for 14 days only, so the CSVs are the long-term record (ADR-017).
"""Usage: collect-stats.py <output dir>

Environment:
  GITHUB_REPOSITORY  owner/name, e.g. TSTS87/garagelog
  GITHUB_TOKEN       token for the public endpoints (releases, repository)
  TRAFFIC_TOKEN      optional token with "Administration: read" for the traffic endpoints;
                     without it only downloads and repository counters are collected
"""

import csv
import datetime
import json
import os
import sys
import urllib.request

API = "https://api.github.com"


def get(path, token):
    """Returns the decoded JSON of a GitHub API GET request."""
    request = urllib.request.Request(API + path, headers={
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
        "X-GitHub-Api-Version": "2022-11-28",
    })
    with urllib.request.urlopen(request) as response:
        return json.load(response)


def merge(path, header, rows, key_columns):
    """Writes rows into a CSV file, replacing existing rows with the same key.

    @param path CSV file; created with header if missing
    @param header column names
    @param rows new rows as lists in header order
    @param key_columns number of leading columns that identify a row; a newer value for the
           same key wins, so re-running a week or a day overwrites instead of duplicating
    """
    table = {}
    if os.path.exists(path):
        with open(path, newline="") as f:
            for row in list(csv.reader(f))[1:]:
                table[tuple(row[:key_columns])] = row
    for row in rows:
        row = [str(value) for value in row]
        table[tuple(row[:key_columns])] = row
    with open(path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(sorted(table.values()))


def main():
    out = sys.argv[1]
    repo = os.environ["GITHUB_REPOSITORY"]
    token = os.environ["GITHUB_TOKEN"]
    traffic_token = os.environ.get("TRAFFIC_TOKEN", "")
    today = datetime.date.today().isoformat()

    # Download counters are totals since upload, so each run records a snapshot per day.
    downloads = []
    for release in get(f"/repos/{repo}/releases?per_page=100", token):
        for asset in release["assets"]:
            downloads.append([today, release["tag_name"], asset["name"], asset["download_count"]])
    merge(os.path.join(out, "downloads.csv"), ["date", "tag", "asset", "downloads_total"], downloads, 3)

    info = get(f"/repos/{repo}", token)
    merge(os.path.join(out, "repository.csv"), ["date", "stars", "forks", "watchers", "open_issues"],
          [[today, info["stargazers_count"], info["forks_count"], info["subscribers_count"],
            info["open_issues_count"]]], 1)

    if not traffic_token:
        print("TRAFFIC_TOKEN not set: skipping views, clones and referrers")
        return

    # Views and clones come per day for the last 14 days; the newest run has the most complete
    # numbers for a day, so merging by date fills gaps and corrects the partial current day.
    for kind in ("views", "clones"):
        data = get(f"/repos/{repo}/traffic/{kind}?per=day", traffic_token)
        rows = [[entry["timestamp"][:10], entry["count"], entry["uniques"]] for entry in data[kind]]
        merge(os.path.join(out, f"{kind}.csv"), ["date", "count", "uniques"], rows, 1)

    # Referrers are only available as a 14-day total, so they are stored as a weekly snapshot.
    referrers = [[today, entry["referrer"], entry["count"], entry["uniques"]]
                 for entry in get(f"/repos/{repo}/traffic/popular/referrers", traffic_token)]
    merge(os.path.join(out, "referrers.csv"), ["date", "referrer", "count_14d", "uniques_14d"], referrers, 2)


if __name__ == "__main__":
    main()

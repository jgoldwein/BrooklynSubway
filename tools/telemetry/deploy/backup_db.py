"""Consistent online backup of the telemetry SQLite database (template helper).

Uses the SQLite online-backup API, which is safe while the collector is writing in
WAL mode (a plain file copy of the .sqlite3 file alone can miss committed data
still in the -wal file). Opens the source read-only and never modifies it.

    python backup_db.py --db /var/lib/brooklyn-map/telemetry.sqlite3 \
        --out /var/backups/brooklyn-map/telemetry-YYYYmmdd.sqlite3

Refuses to overwrite an existing file and verifies the copy with
PRAGMA integrity_check before declaring success.
"""

import argparse
import sqlite3
import sys
from pathlib import Path


def backup(source, dest):
    source, dest = Path(source), Path(dest)
    if not source.is_file():
        raise SystemExit(f"source database not found: {source}")
    if dest.exists():
        raise SystemExit(f"refusing to overwrite existing file: {dest}")
    src = sqlite3.connect(f"file:{source.as_posix()}?mode=ro", uri=True, timeout=30)
    try:
        dst = sqlite3.connect(str(dest))
        try:
            src.backup(dst)
            result = dst.execute("PRAGMA integrity_check").fetchone()[0]
            version = dst.execute("PRAGMA user_version").fetchone()[0]
        finally:
            dst.close()
    finally:
        src.close()
    if result != "ok":
        dest.unlink()
        raise SystemExit(f"backup failed integrity_check: {result}")
    return version


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--db", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    version = backup(args.db, args.out)
    print(f"backup ok (schema version {version}): {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

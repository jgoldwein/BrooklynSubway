#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Offline unit tests for the Phase 4Z aggregate UnknownStop audit."""

import csv
import io
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from unknown_stop_audit import (  # noqa: E402
    AuditError,
    audit_pairs,
    load_replay,
    load_static_gtfs,
    render_report,
    topology_hashes_match,
    write_audit_csv,
)


def write_member(archive, name, header, rows):
    text = io.StringIO(newline="")
    writer = csv.writer(text, lineterminator="\n")
    writer.writerow(header)
    writer.writerows(rows)
    archive.writestr(name, text.getvalue())


class UnknownStopAuditTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="phase4z-audit-test-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.gtfs_path = self.directory / "static.zip"
        with zipfile.ZipFile(self.gtfs_path, "w", zipfile.ZIP_DEFLATED) as archive:
            write_member(
                archive,
                "feed_info.txt",
                ("feed_version",),
                (("fixture-v1",),),
            )
            write_member(
                archive,
                "stops.txt",
                ("stop_id", "stop_name", "stop_lat", "stop_lon", "location_type", "parent_station"),
                (
                    ("100", "Hub", "40.0000", "-73.9000", "1", ""),
                    ("100N", "Hub North", "40.0001", "-73.9000", "0", "100"),
                    ("100S", "Hub South", "39.9999", "-73.9000", "0", "100"),
                    ("099", "Boundary", "39.9990", "-73.9000", "1", ""),
                    ("200", "Museum", "40.1000", "-73.9000", "1", ""),
                    ("200N", "Museum North", "40.1001", "-73.9000", "0", "200"),
                    ("300", "Outside", "41.0000", "-73.9000", "1", ""),
                    ("300N", "Outside North", "41.0001", "-73.9000", "0", "300"),
                ),
            )
            write_member(
                archive,
                "trips.txt",
                ("route_id", "trip_id"),
                (("A", "trip-a"), ("C", "trip-c")),
            )
            write_member(
                archive,
                "stop_times.txt",
                ("trip_id", "stop_id", "stop_sequence"),
                (
                    ("trip-a", "099", "1"),
                    ("trip-a", "100N", "2"),
                    ("trip-a", "300N", "3"),
                    ("trip-c", "200N", "1"),
                ),
            )
        self.static = load_static_gtfs(self.gtfs_path)
        self.static["topology_hash_match"] = False
        self.stop_map = [{
            "gtfs_parent_stop_id": "100",
            "station_id": "7",
            "registry_name": "Hub",
            "gtfs_stop_name": "Hub",
            "registry_routes": "A",
            "lat": "40.0000",
            "lon": "-73.9000",
        }]
        self.led_rows = [{
            "role": "map",
            "element_type": "STATION",
            "route": "A",
            "station_id": "7",
        }]
        self.count_rows = [
            {"route": "A", "stop_id": "100S", "observations_in_10_cycles": "2"},
            {"route": "B", "stop_id": "200N", "observations_in_10_cycles": "3"},
            {"route": "A", "stop_id": "300N", "observations_in_10_cycles": "4"},
        ]
        self.replay_path = self.directory / "replay.json"
        observations = [
            {
                "category": "UnknownStop",
                "route": "A",
                "currentStop": "100S",
                "previousStop": "099",
                "previousStopFailure": "None",
                "previousStopSource": "StaticTopology",
                "nextStop": "100S",
                "tripId": "trip-a",
            }
            for _ in range(2)
        ]
        observations.extend(
            {
                "category": "UnknownStop",
                "route": "B",
                "currentStop": "200N",
                "previousStop": "Unavailable",
                "previousStopFailure": "None",
                "previousStopSource": "Unavailable",
                "nextStop": "200N",
                "tripId": "missing-static-trip",
            }
            for _ in range(3)
        )
        observations.extend(
            {
                "category": "UnknownStop",
                "route": "A",
                "currentStop": "300N",
                "previousStop": "Unavailable",
                "previousStopFailure": "UnknownDestination",
                "previousStopSource": "Unavailable",
                "nextStop": "300N",
                "tripId": "trip-a",
            }
            for _ in range(4)
        )
        self.replay_path.write_text(
            json.dumps({
                "schema": "brooklyn-gtfsrt-replay",
                "schemaVersion": 1,
                "frames": [{"observations": observations}],
            }),
            encoding="utf-8",
        )

    def test_feed_service_and_platform_parent_join(self):
        self.assertEqual(self.static["feed_version"], "fixture-v1")
        self.assertEqual(self.static["served"]["100"], {"A"})
        self.assertEqual(self.static["served"]["200"], {"C"})

    def test_missing_observed_stop_is_rejected(self):
        count_rows = [{"route": "A", "stop_id": "999N", "observations_in_10_cycles": "1"}]
        with self.assertRaises(AuditError):
            audit_pairs(count_rows, self.static, self.stop_map, self.led_rows)

    def test_topology_source_hashes_are_checked(self):
        path = self.directory / "topology.csv"
        path.write_text(
            "# sha256_stops.txt=wrong\n# sha256_trips.txt=wrong\n"
            "# sha256_stop_times.txt=wrong\n",
            encoding="utf-8",
        )
        self.assertFalse(topology_hashes_match(self.static, path))

    def test_classifies_map_boundary_route_disagreement_and_off_map(self):
        replay = load_replay(self.replay_path)
        rows = audit_pairs(
            self.count_rows,
            self.static,
            self.stop_map,
            self.led_rows,
            replay,
        )
        by_stop = {row["platform_stop"]: row for row in rows}
        self.assertEqual(
            by_stop["100S"]["classification"],
            "MAP_BOUNDARY_SEGMENT_PREVIOUS_STOP_OFF_MAP",
        )
        self.assertEqual(by_stop["100S"]["previous_stop_ids"], "099")
        self.assertEqual(by_stop["100S"]["route_station_led"], "yes")
        self.assertEqual(by_stop["200N"]["classification"], "STATIC_ROUTE_SERVICE_MISMATCH")
        self.assertEqual(by_stop["200N"]["static_gtfs_routes"], "C")
        self.assertEqual(by_stop["300N"]["classification"], "OUTSIDE_ROUTE_MAP_SCOPE")
        self.assertEqual(by_stop["300N"]["unknown_destination_observations"], 4)

    def test_replay_totals_must_match_count_csv(self):
        replay = load_replay(self.replay_path)
        invalid = [dict(row) for row in self.count_rows]
        invalid[0]["observations_in_10_cycles"] = "1"
        with self.assertRaises(AuditError):
            audit_pairs(invalid, self.static, self.stop_map, self.led_rows, replay)

    def test_csv_and_report_are_deterministic_and_omit_trip_ids(self):
        replay = load_replay(self.replay_path)
        rows = audit_pairs(
            self.count_rows,
            self.static,
            self.stop_map,
            self.led_rows,
            replay,
        )
        csv_a = self.directory / "a.csv"
        csv_b = self.directory / "b.csv"
        write_audit_csv(csv_a, rows)
        write_audit_csv(csv_b, rows)
        self.assertEqual(csv_a.read_bytes(), csv_b.read_bytes())
        output = csv_a.read_text(encoding="utf-8")
        self.assertNotIn("trip-a", output)
        self.assertNotIn("missing-static-trip", output)
        report_a = render_report(rows, self.static, Path("counts.csv"), Path("replay.json"), Path("audit.csv"))
        report_b = render_report(rows, self.static, Path("counts.csv"), Path("replay.json"), Path("audit.csv"))
        self.assertEqual(report_a, report_b)
        self.assertIn("STATIC_ROUTE_SERVICE_MISMATCH", report_a)
        self.assertIn("100 `OperationalStop`", report_a)
        self.assertIn("| Parent | Station | N | S | Total |", report_a)

    def test_every_project_aggregate_count_has_expected_shape(self):
        counts_path = Path(__file__).resolve().parents[2] / "docs" / "PHASE4Y_UNKNOWN_STOP_COUNTS.csv"
        with counts_path.open(encoding="utf-8-sig", newline="") as handle:
            counts = list(csv.DictReader(handle))
        self.assertEqual(len(counts), 152)
        self.assertEqual(sum(int(row["observations_in_10_cycles"]) for row in counts), 429)


if __name__ == "__main__":
    unittest.main()

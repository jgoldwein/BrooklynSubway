#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Independently validate generated semantic tables against Phase 3B sources.

Re-reads the generated C++ (not the generator's in-memory data), compares it with
the CSV sources, the accepted physical chain, and src/led_map.cpp, runs the
human-confirmed spot checks, exercises the generator's failure gates, and prints
a static size estimate.

Usage: python tools/validate_semantic_tables.py
"""

import csv
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_semantic_tables as gen  # noqa: E402

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        print(f"FAIL: {message}")


def unescape(literal):
    raw = bytearray()
    i = 0
    while i < len(literal):
        ch = literal[i]
        if ch == "\\":
            nxt = literal[i + 1]
            if nxt.isdigit():
                raw.append(int(literal[i + 1:i + 4], 8))
                i += 4
                continue
            raw.append(ord(nxt))
            i += 2
            continue
        raw.append(ord(ch))
        i += 1
    return raw.decode("utf-8")


def parse_generated():
    cpp = gen.OUT_CPP.read_text(encoding="utf-8")
    header = gen.OUT_H.read_text(encoding="utf-8")
    routes_enum = {name: int(val) for name, val in
                   re.findall(r"^\s+(Route\w+) = (\d+),", header, re.M)}
    leds = []
    for m in re.finditer(
            r"^\s+\{(\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), "
            r"Route::(\w+), Role::(\w+)\},", cpp, re.M):
        leds.append(dict(zip(
            ["designator", "global", "station", "a", "b", "channel", "index"],
            map(int, m.groups()[:7])), route=m.group(8), role=m.group(9)))
    stations = []
    for m in re.finditer(r'^\s+\{(\d+), "((?:[^"\\]|\\.)*)", (nullptr|"(?:[^"\\]|\\.)*")\},',
                         cpp, re.M):
        gtfs = None if m.group(3) == "nullptr" else unescape(m.group(3)[1:-1])
        stations.append((int(m.group(1)), unescape(m.group(2)), gtfs))
    route_names = re.search(r"kRouteNames\[kRouteCount \+ 1\] = \{(.*?)\};", cpp).group(1)
    route_names = [unescape(s) for s in re.findall(r'"([^"]*)"', route_names)]
    return cpp, header, routes_enum, leds, stations, route_names


def load_led_map_designators():
    text = (ROOT / "src/led_map.cpp").read_text(encoding="utf-8")
    block = re.search(r"DESIGNATORS\[TOTAL_LED_COUNT\] = \{(.*?)\};", text, re.S).group(1)
    return [int(x) for x in re.findall(r"\d+", block)]


def validate_content():
    cpp, header, routes_enum, leds, stations, route_names = parse_generated()
    ws = gen.read_csv(gen.WORKSHEET)
    reg = gen.read_csv(gen.REGISTRY)
    chain = gen.load_chain()
    map_rows = {r["designator"]: r for r in ws if r["role"] == "map"}

    check("AUTO-GENERATED -- DO NOT EDIT" in cpp and "AUTO-GENERATED -- DO NOT EDIT" in header,
          "generated header banner missing")
    check(gen.BASELINE_TAG in cpp, "baseline tag missing from generated source")
    check(not re.search(r"20\d\d-\d\d-\d\d[T ]\d\d:\d\d", cpp + header),
          "generated files contain a timestamp")

    n_station = sum(1 for l in leds if l["role"] == "Station")
    n_segment = sum(1 for l in leds if l["role"] == "Segment")
    check(len(leds) == 302, f"parsed {len(leds)} map LED records, expected 302")
    check(n_station == 276, f"parsed {n_station} STATION, expected 276")
    check(n_segment == 26, f"parsed {n_segment} SEGMENT, expected 26")
    check(len(stations) == 188, f"parsed {len(stations)} registry entities, expected 188")
    check([s[0] for s in stations] == list(range(1, 189)), "registry IDs not contiguous 1..188")

    ids = {s[0] for s in stations}
    registry_by_id = {int(r["station_id"]): r for r in reg}
    for sid, name, gtfs in stations:
        row = registry_by_id[sid]
        check(name == row["display_name"], f"registry {sid} name mismatch")
        check(gtfs == (row["gtfs_parent_stop_id"].strip() or None),
              f"registry {sid} gtfs mismatch (GTFS IDs must be copied, never invented)")

    check(route_names == [""] + gen.ROUTES, "route name table mismatch")
    check(sorted(routes_enum.values()) == list(range(0, len(gen.ROUTES) + 1)) or
          sorted(routes_enum.values()) == list(range(1, len(gen.ROUTES) + 1)),
          "route enum values not dense")

    seen_addr, seen_des = set(), set()
    for l in leds:
        des = f"LED{l['designator']}"
        src = map_rows.get(des)
        check(src is not None, f"{des} not a worksheet map LED")
        if src is None:
            continue
        check(des not in gen.STATUS_DESIGNATORS, f"status LED {des} in generated table")
        seen_des.add(des)
        seen_addr.add((l["channel"], l["index"]))
        check((l["channel"], l["index"], l["global"]) ==
              (int(src["channel"]), int(src["channel_index"]),
               int(src["global_physical_order"])), f"{des} physical identity mismatch")
        check(chain[l["global"]] == (l["channel"], l["index"], des, "map"),
              f"{des} differs from accepted chain")
        check(l["route"] == f"Route{src['route']}", f"{des} route mismatch")
        if l["role"] == "Station":
            check(src["element_type"] == "STATION" and l["station"] == int(src["station_id"])
                  and l["a"] == 0 and l["b"] == 0 and l["station"] in ids,
                  f"{des} station record mismatch")
        else:
            check(src["element_type"] == "SEGMENT" and l["station"] == 0
                  and l["a"] == int(src["from_station_id"])
                  and l["b"] == int(src["to_station_id"])
                  and l["a"] in ids and l["b"] in ids and l["a"] != l["b"],
                  f"{des} segment record mismatch")
    check(len(seen_des) == 302 and len(seen_addr) == 302,
          "designators/channel-index identities not unique across 302 records")
    check(seen_des == set(map_rows), "generated designators != worksheet map LED set")
    check("U23" not in cpp, "U23 present in generated source")
    check([l["global"] for l in leds] == sorted(l["global"] for l in leds),
          "records not sorted by global order")

    # One-to-one with the physical chain actually compiled into the firmware.
    firmware_designators = load_led_map_designators()
    map_global = {l["global"]: l["designator"] for l in leds}
    check(len(firmware_designators) == 306, "led_map.cpp designator table size != 306")
    for g, des in map_global.items():
        check(firmware_designators[g] == des, f"global {g}: led_map.cpp has "
              f"LED{firmware_designators[g]}, generated LED{des}")
    check({firmware_designators[g] for g in range(306)} - set(map_global.values()) ==
          {252, 251, 306, 307}, "non-map LEDs in led_map.cpp are not exactly the 4 status LEDs")

    # Reverse index consistency.
    offs = [int(x) for x in re.search(
        r"kStationLedOffsets\[kRegistryCount \+ 1\] = \{(.*?)\};", cpp, re.S).group(1).replace(
        "\n", " ").replace(",", " ").split()]
    flat = [int(x) for x in re.search(
        r"kStationLedRecords\[kStationLedCount\] = \{(.*?)\};", cpp, re.S).group(1).replace(
        "\n", " ").replace(",", " ").split()]
    check(len(offs) == 189 and offs[0] == 0 and offs[-1] == 276 and len(flat) == 276,
          "station reverse index shape wrong")
    for sid in range(1, 189):
        for rec in flat[offs[sid - 1]:offs[sid]]:
            check(leds[rec]["role"] == "Station" and leds[rec]["station"] == sid,
                  f"reverse index entry {rec} wrong for station {sid}")
    check(sorted(flat) == sorted(i for i, l in enumerate(leds) if l["role"] == "Station"),
          "reverse index does not cover exactly the STATION records")

    # Human-confirmed spot checks.
    name = {sid: n for sid, n, _ in stations}
    by_des = {f"LED{l['designator']}": l for l in leds}

    def station_is(des, route, station):
        l = by_des[des]
        check(l["role"] == "Station" and l["route"] == f"Route{route}"
              and name[l["station"]] == station, f"spot check {des} {route}/{station}")

    def segment_is(des, route, a, b):
        l = by_des[des]
        check(l["role"] == "Segment" and l["route"] == f"Route{route}"
              and (name[l["a"]], name[l["b"]]) == (a, b),
              f"spot check {des} {route} {a}->{b}")

    station_is("LED96", "G", "Lorimer St")
    segment_is("LED299", "L", "Broadway Junction", "Atlantic Av")
    segment_is("LED291", "F", "York St", "East Broadway")
    segment_is("LED304", "S", "Botanic Garden", "Park Pl")
    segment_is("LED287", "A", "Hoyt-Schermerhorn Sts", "Nostrand Av")
    segment_is("LED285", "B", "Grand St", "DeKalb Av")
    segment_is("LED286", "D", "Grand St", "Atlantic Av-Barclays Ctr")
    check(by_des["LED308"]["role"] == "Segment" and by_des["LED308"]["route"] == "RouteD",
          "spot check LED308 D segment")
    check(by_des["LED309"]["role"] == "Segment" and by_des["LED309"]["route"] == "RouteB",
          "spot check LED309 B segment")
    return leds, stations, cpp


def make_workspace(mutator):
    tmp = Path(tempfile.mkdtemp())
    for src in (gen.WORKSHEET, gen.REGISTRY, gen.CHAIN):
        dst = tmp / src.relative_to(gen.ROOT)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(src, dst)
    mutator(tmp)
    return tmp


def edit_csv(tmp, path, fn):
    p = tmp / path.relative_to(gen.ROOT)
    with open(p, newline="", encoding="utf-8-sig") as h:
        rows = list(csv.DictReader(h))
        fields = rows and list(rows[0].keys())
    fn(rows)
    with open(p, "w", newline="", encoding="utf-8") as h:
        w = csv.DictWriter(h, fieldnames=fields, quoting=csv.QUOTE_ALL)
        w.writeheader()
        w.writerows(rows)


def expect_generation_failure(label, path, fn):
    tmp = make_workspace(lambda t: edit_csv(t, path, fn))
    saved = (gen.WORKSHEET, gen.REGISTRY, gen.CHAIN)
    try:
        gen.WORKSHEET = tmp / saved[0].relative_to(gen.ROOT)
        gen.REGISTRY = tmp / saved[1].relative_to(gen.ROOT)
        gen.CHAIN = tmp / saved[2].relative_to(gen.ROOT)
        try:
            gen.build()
        except gen.GenerationError as error:
            print(f"  gate ok: {label}: {error}")
            return
        check(False, f"generator accepted invalid input: {label}")
    finally:
        gen.WORKSHEET, gen.REGISTRY, gen.CHAIN = saved
        shutil.rmtree(tmp, ignore_errors=True)


def row(rows, des):
    return next(r for r in rows if r["designator"] == des)


def negative_tests():
    W, R = gen.WORKSHEET, gen.REGISTRY
    expect_generation_failure("unresolved row", W,
                              lambda rs: row(rs, "LED96").update(semantic_status="CANDIDATE"))
    expect_generation_failure("dropped map LED", W, lambda rs: rs.remove(row(rs, "LED96")))
    expect_generation_failure("duplicate channel/index", W,
                              lambda rs: row(rs, "LED97").update(
                                  channel=row(rs, "LED96")["channel"],
                                  channel_index=row(rs, "LED96")["channel_index"]))
    expect_generation_failure("missing channel", W,
                              lambda rs: row(rs, "LED96").update(channel=""))
    expect_generation_failure("unsupported route", W,
                              lambda rs: row(rs, "LED96").update(route="X"))
    expect_generation_failure("station missing registry id", W,
                              lambda rs: row(rs, "LED96").update(station_id="9999"))
    expect_generation_failure("segment endpoint missing registry id", W,
                              lambda rs: row(rs, "LED299").update(to_station_id="9999"))
    expect_generation_failure("segment A == B", W,
                              lambda rs: row(rs, "LED299").update(
                                  to_station_id=row(rs, "LED299")["from_station_id"],
                                  to_station=row(rs, "LED299")["from_station"],
                                  to_station_key=row(rs, "LED299")["from_station_key"]))
    expect_generation_failure("status LED as map", W,
                              lambda rs: row(rs, "LED252").update(role="map"))
    expect_generation_failure("U23 added", W,
                              lambda rs: rs.append(dict(row(rs, "LED252"), designator="U23")))
    expect_generation_failure("registry entity dropped", R, lambda rs: rs.pop())


def size_estimate(cpp):
    strings = sum(len(unescape(s).encode("utf-8")) + 1 for s in
                  re.findall(r'"((?:[^"\\]|\\.)*)"', cpp.split("kStations[kRegistryCount]")[1]
                             .split("const MapLed kMapLeds")[0]))
    route_strings = sum(len(r) + 1 for r in [""] + gen.ROUTES)
    stations = 188 * 12          # id(2)+pad(2)+2 pointers(4+4) on 32-bit Xtensa
    leds = 302 * 14
    offsets = 189 * 2
    index = 276 * 2
    routes_tbl = 19 * 4
    total = strings + route_strings + stations + leds + offsets + index + routes_tbl
    print("Static data estimate (flash .rodata, 32-bit ESP32-S3; RAM = 0 bytes, no heap):")
    print(f"  kMapLeds                 {leds:6d} B (302 x 14)")
    print(f"  kStations                {stations:6d} B (188 x 12)")
    print(f"  station name/GTFS text   {strings:6d} B")
    print(f"  kStationLedOffsets       {offsets:6d} B")
    print(f"  kStationLedRecords       {index:6d} B")
    print(f"  kRouteNames + text       {routes_tbl + route_strings:6d} B")
    print(f"  TOTAL                    {total:6d} B (~{total / 1024:.1f} KiB)")


def main():
    print("Regenerating check...")
    sys.argv = [sys.argv[0], "--check"]
    check(gen.main() == 0, "generated files are stale vs. sources")
    leds, stations, cpp = validate_content()
    print("Generator failure gates:")
    negative_tests()
    size_estimate(cpp)
    if failures:
        print(f"\nVALIDATION FAILED: {len(failures)} problem(s)")
        return 1
    print(f"\nVALIDATION PASSED: {len(leds)} map LEDs "
          f"({sum(1 for l in leds if l['role'] == 'Station')} STATION, "
          f"{sum(1 for l in leds if l['role'] == 'Segment')} SEGMENT), "
          f"{len(stations)} registry entities")
    return 0


if __name__ == "__main__":
    sys.exit(main())

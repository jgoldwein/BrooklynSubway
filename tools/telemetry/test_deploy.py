# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Static tests for the Phase 4AA deployment templates (nothing is installed or run)."""

import re
import sqlite3
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEPLOY = ROOT / "tools/telemetry/deploy"
sys.path.insert(0, str(DEPLOY))

import backup_db  # noqa: E402

CHECKS = 0
FAILURES = 0


def check(condition, description):
    global CHECKS, FAILURES
    CHECKS += 1
    if not condition:
        FAILURES += 1
        print("FAIL:", description)


def read(name):
    return (DEPLOY / name).read_text(encoding="utf-8")


def directive(text, key):
    return [m.group(1).strip() for m in re.finditer(rf"^{key}=(.*)$", text, re.M)]


def test_units():
    collector = read("brooklyn-telemetry-collector.service")
    dashboard = read("brooklyn-telemetry-dashboard.service")
    env = read("telemetry.env.example")
    exec_collector = directive(collector, "ExecStart")[0]
    exec_dashboard = directive(dashboard, "ExecStart")[0]

    check(directive(collector, "User") not in ([], ["root"]), "collector runs as a non-root user")
    check(directive(dashboard, "User") not in ([], ["root"]), "dashboard runs as a non-root user")
    check(directive(collector, "User") != directive(dashboard, "User"), "collector and dashboard use different service users")
    check(directive(collector, "EnvironmentFile") == ["/etc/brooklyn-map/telemetry.env"], "collector references the EnvironmentFile")
    check(not directive(dashboard, "EnvironmentFile"), "dashboard needs no credentials file")
    check(directive(collector, "WorkingDirectory") and directive(dashboard, "WorkingDirectory"), "explicit WorkingDirectory")
    check(directive(collector, "Restart") == ["on-failure"] and directive(dashboard, "Restart") == ["on-failure"], "restart on failure")
    check(directive(collector, "RestartSec") and "StartLimitBurst=" in collector, "bounded restart delay and rate limit")
    check("--db /var/lib/brooklyn-map/telemetry.sqlite3" in exec_collector, "collector database path explicit")
    check("sh -c" not in exec_collector and "bash" not in exec_collector, "no shell wrapper in ExecStart")
    check(not re.search(r"(?i)password|passwd|secret|token", exec_collector + exec_dashboard), "no credential words in ExecStart")
    check("${BROOKLYN_MQTT_PORT}" in exec_collector and "BROOKLYN_MQTT_PORT=8884" in env, "collector port comes from env, set to 8884")
    check("1883" not in collector + dashboard + env, "no plaintext MQTT port in templates")
    check("0.0.0.0" not in dashboard + collector and "--host" not in exec_dashboard, "dashboard has no non-loopback bind")
    check("NoNewPrivileges=true" in collector and "NoNewPrivileges=true" in dashboard, "NoNewPrivileges set")
    check("ProtectSystem=strict" in collector and "ProtectSystem=strict" in dashboard, "filesystem protected")
    check("ReadWritePaths=/var/lib/brooklyn-map" in collector, "collector writes only its state dir")
    check("ReadOnlyPaths=/var/lib/brooklyn-map" in dashboard and "ReadWritePaths" not in dashboard, "dashboard is read-only on the database")
    check("IPAddressAllow=localhost" in dashboard, "dashboard network access limited to loopback")


def test_env_and_secrets():
    env = read("telemetry.env.example")
    values = dict(line.split("=", 1) for line in env.splitlines() if "=" in line and not line.startswith("#"))
    check(values.get("BROOKLYN_MQTT_HOST") == "mqtt.goldwein.net", "env host is the broker name")
    check(values.get("BROOKLYN_MQTT_COLLECTOR_USER") == "REPLACE_ME", "collector user is a placeholder")
    check(values.get("BROOKLYN_MQTT_COLLECTOR_PASSWORD") == "REPLACE_ME", "collector password is a placeholder")
    check("chmod 600" in env and "NEVER commit" in env, "env template documents permissions and no-commit rule")
    for path in sorted(p for p in DEPLOY.iterdir() if p.is_file()):
        if path.suffix in (".py",):
            continue
        text = path.read_text(encoding="utf-8")
        check(not re.search(r"\$2[aby]\$|\$apr1\$", text), f"{path.name} contains no password hash")
        check(not re.search(r"(?i)\bgoldwein\b\s*[:=]", text) and "BROOKLYN_MQTT_OBSERVER" not in text,
              f"{path.name} does not embed the admin/observer account")
        check("setinsecure" not in text.lower() and "tls_insecure" not in text.lower() and "cert_none" not in text.lower(),
              f"{path.name} has no insecure TLS fallback")
        check(not re.search(r"(?i)(password|passwd)\s*=\s*(?!REPLACE_ME|\s*$)\S", text), f"{path.name} has no literal password")


def test_apache():
    conf = read("apache-monitor.conf.example")
    check("https://monitor.goldwein.net/" in conf and "SSLEngine on" in conf, "HTTPS vhost")
    check("AuthType Basic" in conf and "Require valid-user" in conf and "htpasswd -B" in conf, "Basic Auth with bcrypt guidance")
    check("ProxyPass        / http://127.0.0.1:8780/" in conf, "reverse proxy targets loopback")
    check(not re.search(r"ProxyPass\s+\S+\s+http://(?!127\.0\.0\.1)", conf), "no proxy target beyond loopback")
    check("Strict-Transport-Security" in conf and "X-Content-Type-Options" in conf, "security headers")
    check("fail2ban" in conf, "rate-limiting recommendation present")
    check("LimitExcept GET HEAD" in conf, "mutating methods refused")


def test_backup_helper():
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / "src.sqlite3"
        db = sqlite3.connect(src)
        db.execute("PRAGMA journal_mode=WAL")
        db.execute("CREATE TABLE t (x INTEGER)")
        db.execute("INSERT INTO t VALUES (42)")
        db.execute("PRAGMA user_version=2")
        db.commit()
        # The row is committed but may live only in the -wal file; backup must still include it.
        out = Path(tmp) / "out.sqlite3"
        check(backup_db.backup(src, out) == 2, "backup preserves schema version")
        copy = sqlite3.connect(out)
        try:
            check(copy.execute("SELECT x FROM t").fetchone() == (42,), "backup includes WAL-resident committed data")
        finally:
            copy.close()
        try:
            backup_db.backup(src, out)
            refused = False
        except SystemExit:
            refused = True
        check(refused, "backup refuses to overwrite an existing file")
        try:
            backup_db.backup(Path(tmp) / "missing.sqlite3", Path(tmp) / "x.sqlite3")
            missing = False
        except SystemExit:
            missing = True
        check(missing, "backup reports a missing source")
        db.close()


def main():
    for test in (test_units, test_env_and_secrets, test_apache, test_backup_helper):
        test()
    print(f"Telemetry deployment template tests: {CHECKS} checks, {FAILURES} failures")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())

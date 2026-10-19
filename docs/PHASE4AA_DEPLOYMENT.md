# Phase 4AA host deployment guide (templates only; nothing is deployed)

Everything here is documentation plus templates in
[tools/telemetry/deploy/](../tools/telemetry/deploy/). No script in this repository
installs, starts, or reconfigures anything on a server. Static checks:
`tools/telemetry/test_deploy.py` (run by `python tools/run_telemetry_tests.py`).

```
Internet -> Apache HTTPS (monitor.goldwein.net) -> Basic Auth -> reverse proxy
         -> dashboard.py on 127.0.0.1:8780 (read-only) -> SQLite <- collect.py <- MQTT 8884 TLS
```

## Recommended Linux layout

| Path | Purpose | Owner / mode |
| --- | --- | --- |
| `/opt/brooklyn-map/telemetry/` | Checkout of this repository (code) | `root:root`, `0755`; not writable by service users |
| `/opt/brooklyn-map/venv/` | Python venv with `paho-mqtt==2.1.0` | `root:root`, read/execute for all |
| `/etc/brooklyn-map/telemetry.env` | Collector credentials (from `telemetry.env.example`) | `root:root`, `0600` |
| `/var/lib/brooklyn-map/telemetry.sqlite3` (+ `-wal`, `-shm`) | Database | dir `brooklyn-collector:brooklyn-telemetry` `0750`; files `0640` |
| `/var/backups/brooklyn-map/` | Backups | `brooklyn-collector:brooklyn-collector`, `0700` |
| `/etc/systemd/system/brooklyn-telemetry-*.service` | Units | `root:root`, `0644` |
| `/etc/apache2/brooklyn-monitor.htpasswd` | bcrypt Basic Auth file (never in Git) | `root:www-data`, `0640` |

Service accounts: `brooklyn-collector` (writes the DB, holds MQTT access through
the env file) and `brooklyn-dashboard` (reads only, no credentials). Both are
no-login system users in group `brooklyn-telemetry`.

## Deployment checklist (operator, after review)

1. Record the Mosquitto version and confirm `bkmap-collector` ACL (read-only on `brooklyn/v1/+/{status,event,availability}`).
2. Create users/group: `sudo groupadd --system brooklyn-telemetry; sudo useradd --system --no-create-home --shell /usr/sbin/nologin -g brooklyn-telemetry brooklyn-collector`; same for `brooklyn-dashboard`.
3. Check out the repo at a reviewed commit/tag into `/opt/brooklyn-map/telemetry` (root-owned).
4. `python3 -m venv /opt/brooklyn-map/venv && /opt/brooklyn-map/venv/bin/pip install -r /opt/brooklyn-map/telemetry/tools/telemetry/requirements.txt`.
5. Create `/etc/brooklyn-map/telemetry.env` from the template, fill in locally, `chmod 600`.
6. Install the two unit files (adjust paths if you chose different ones); `systemctl daemon-reload`. The collector unit's `StateDirectory=` creates `/var/lib/brooklyn-map`.
7. `systemctl enable --now brooklyn-telemetry-collector`, then the dashboard unit.
8. Verify: `systemctl status`, `journalctl -u brooklyn-telemetry-collector` (outcome categories only; no payloads or credentials are logged), `curl -s http://127.0.0.1:8780/api/devices`, `ss -ltn | grep 8780` shows `127.0.0.1` only.
9. Install the Apache vhost from the template, create the bcrypt htpasswd (`htpasswd -B`), `apachectl configtest`, reload. Add fail2ban for failed logins.
10. Schedule backups (below). Check `https://monitor.goldwein.net/` requires login and rejects plain HTTP.

Rollback: `systemctl disable --now` both units, remove the vhost and reload Apache,
keep the database and env file (or securely delete them if credentials are suspect and rotate them on the broker).

### Known item to verify on the real host

The dashboard opens the database read-only (`mode=ro`) while the collector holds
it open in WAL mode. SQLite readers normally need to read the `-shm` file; with
group-readable files this works on SQLite 3.22+ as long as the collector keeps the
database open. If the dashboard returns 503 on the host, the documented fallback is
to give the dashboard group write access to the directory (weaker isolation) or to
run it under the same user. This could not be proven off-host.

## SQLite backup and recovery

- **Backup:** use `tools/telemetry/deploy/backup_db.py --db ... --out ...` (SQLite
  online-backup API, source opened read-only, integrity-checked, refuses to
  overwrite). Do not copy only `telemetry.sqlite3` while the collector runs: in WAL
  mode committed data may still be in `-wal`. A cron/systemd-timer job is an operator choice
  (daily is ample; the data is operational, not critical). Keep ~7 copies.
- **Restore:** stop the collector and dashboard, move the damaged files (including `-wal`/`-shm`) aside, copy the backup to the DB path with the same owner/mode, start the collector, then the dashboard.
- **Schema compatibility:** the DB carries `PRAGMA user_version` (currently 2). The collector migrates older DBs atomically on start; a DB newer than the code is refused, and the read-only dashboard refuses a version mismatch (503).
- **Corruption:** run `PRAGMA integrity_check`; if it fails, restore the last backup or start empty. An empty DB is a valid recovery.
- **What is lost:** at most the events since the last backup (events are retained 30 days). Latest device state is **self-healing**: it regenerates from the next heartbeat (default 30 s) and the broker's retained status/availability, but retained status is only used as "last known" and never counts as live. Historical events cannot be regenerated because devices keep no durable queue.

## Host deployment security checklist

| Item | Template status |
| --- | --- |
| Least privilege: no root, `NoNewPrivileges`, empty capability set, `ProtectSystem=strict`, `PrivateTmp` | in both units |
| Dashboard loopback-only | `dashboard.py` binds 127.0.0.1 with no option to change; unit adds `IPAddressDeny=any`/`IPAddressAllow=localhost` |
| TLS terminates at Apache; HTTP redirects to HTTPS; HSTS | vhost template |
| Authentication before proxy; bcrypt htpasswd; failed-login throttling | vhost template + fail2ban recommendation (operator must configure) |
| Credentials in protected `EnvironmentFile`, not in `ExecStart` | collector unit (verified by `test_deploy.py`) |
| Collector writes only the DB directory | `ReadWritePaths` |
| No shell command exposure or MQTT publishing from the host | collector never publishes; no shell in `ExecStart`; dashboard is GET/HEAD only |
| Secrets not in process arguments | no credentials on any command line |
| Predictable logging, bounded restarts | journald; `Restart=on-failure`, `RestartSec=15`, start limit 5/5 min |
| Collector identity is read-only; `goldwein` never used | broker ACL + env template placeholders |
| Dashboard data | no SSID, passwords, tokens, IPs, trip/vehicle data stored or shown (privacy tests) |

Residual risks: Basic Auth is a shared-secret model (acceptable for one operator
over HTTPS; see decision memo 1 for alternatives); the Apache account store and
fail2ban setup are the operator's responsibility; hardening directives should be
validated with `systemd-analyze security` on the real host.

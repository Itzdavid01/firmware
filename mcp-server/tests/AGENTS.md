# mcp-server/tests/ — Pytest Hardware Integration Suite

**Tiered pytest suite.** Runs against real USB-connected Meshtastic devices. Auto-detects VID→role mapping.

## Tier Execution Order

(enforced via `pytest_collection_modifyitems`)

```
unit → mesh → telemetry → monitor → recovery → ui → fleet → admin → provisioning
```

State-mutating tiers run last to avoid contaminating read-only tiers.

## Tiers

| Tier            | Hardware | What it tests                                                                            |
| --------------- | -------- | ---------------------------------------------------------------------------------------- |
| `unit/`         | None     | Python-only: boards parse, pio wrapper, userPrefs parse, testing profile, uhubctl parser |
| `mesh/`         | 2-device | Bidirectional send, broadcast, direct+ACK, mesh formation (60s), peer offline recovery   |
| `telemetry/`    | 2-device | DEVICE_METRICS_APP broadcast timing                                                      |
| `monitor/`      | 1-device | Boot log panic check                                                                     |
| `recovery/`     | 2-device | uhubctl power-cycle + NVS persistence across hard reset                                  |
| `ui/`           | 2-device | Input-broker-driven screen nav with camera+OCR evidence                                  |
| `fleet/`        | 2-device | PSK seed session isolation                                                               |
| `admin/`        | 2-device | Channel URL roundtrip, owner persistence across reboot                                   |
| `provisioning/` | 2-device | Region/channel bake, admin key, factory_reset survival                                   |

## Key Fixtures (conftest.py)

- `_session_userprefs` — snapshots `userPrefs.jsonc`, merges test profile, restores at teardown (4 layers of safety)
- `_firmware_log_stream` — subscribes to `meshtastic.log.line` → `fwlog.jsonl`
- `_debug_log_buffer` — captures last 200 firmware log lines + device state on failure
- `hub_devices` — `dict[role, SerialInterface]` with session-long exclusive port locks
- `baked_mesh` — parametrized mesh pair fixture (auto-generates `[nrf52→esp32s3]`, `[esp32s3→nrf52]`)
- `test_profile` — session-scoped dict: region, primary channel, admin key, PSK seed

## Invocation

```bash
./run-tests.sh                      # full suite (auto-bake-if-needed)
./run-tests.sh --force-bake         # reflash before testing
./run-tests.sh --assume-baked       # skip bake (dev loop)
./run-tests.sh tests/mesh           # one tier
./run-tests.sh -k telemetry         # name filter
```

## Artifacts

- `report.html` — pytest-html with "Meshtastic debug" section on failure
- `junit.xml` — CI-parseable
- `reportlog.jsonl` — pytest-reportlog stream (TUI input)
- `fwlog.jsonl` — firmware log mirror
- `ui_captures/<session>/<test>/` — PNG + OCR + transcript per UI test step

## NEVER do during tests

```
factory_reset, erase_and_flash, reboot, shutdown mid-test
edit userPrefs.jsonc from inside a test
parallelize MCP calls on same port
```

## Device Discovery

VID→role: `0x239A`→nrf52, `0x303A`/`0x10C4`→esp32s3
Env override: `MESHTASTIC_MCP_ENV_NRF52`, `MESHTASTIC_MCP_ENV_ESP32S3`

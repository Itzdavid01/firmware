# GEMINI.md

This file provides guidance to Google Gemini when working with code in this repository.

## Canonical docs to read first

- **`.github/copilot-instructions.md`** — primary agent-facing reference. Covers project layout, coding conventions, module framework, Observer pattern, encryption, build system, CI, native test suite, full **MCP Server & Hardware Test Harness** contract. Read top-to-bottom before any non-trivial change.
- **`AGENTS.md`** (repo root) — quick command reference, slash commands, MCP tool groups, house rules, recovery one-liners, env-var table. Pointer + cheat sheet over `copilot-instructions.md`.
- **`mcp-server/README.md`** — MCP tool argument shapes, setup, test-suite invocation.
- **`mcp-server/AGENTS.md`** — MCP server internals: module responsibilities, tool surface (43 tools), test tiers, fixtures, CLI flags, port discovery, adding new tools.
- **`docs/lilygo-epaper.md`** — LilyGO e-paper technical reference (GxEPD2/FastEPD, pinouts, variants).
- **`test/README.md`** — native unit-test authoring guide.
- Nested `AGENTS.md` under `variants/`, `mcp-server/`, `src/mesh/`, `src/modules/Telemetry/Sensor/` — subsystem guidance.

No duplicate those files here. Conflict → `.github/copilot-instructions.md` wins.

## Build / test / flash (quick reference)

| Action                              | Command                                                                                        |
| ----------------------------------- | ---------------------------------------------------------------------------------------------- |
| Build variant                       | `pio run -e <env>` (e.g. `rak4631`, `heltec-v3`, `t-deck-pro-voice`)                           |
| Clean + rebuild                     | `pio run -e <env> -t clean && pio run -e <env>`                                                |
| Flash                               | `pio run -e <env> -t upload --upload-port <port>` — or the `pio_flash` MCP tool                |
| Native unit tests                   | `pio test -e native`                                                                           |
| Single native test suite            | `pio test -e native -f test_crypto`                                                            |
| Hardware regression suite           | `./mcp-server/run-tests.sh`                                                                    |
| One tier / one file / name filter   | `./mcp-server/run-tests.sh tests/mesh` / `tests/mesh/test_direct_with_ack.py` / `-k telemetry` |
| Live TUI test runner                | `mcp-server/.venv/bin/meshtastic-mcp-test-tui`                                                 |
| Format (must pass CI `trunk_check`) | `trunk fmt`                                                                                    |
| Regenerate protobuf bindings        | `bin/regen-protos.sh`                                                                          |

MCP server setup (once): `cd mcp-server && python3 -m venv .venv && .venv/bin/pip install -e '.[test]'`. `.mcp.json` at repo root registers the server.

## Flash + Debug Logging Workflow (agents)

When you need to flash firmware and capture device logs for analysis, follow this workflow:

### Step 1 — Detect the device

```bash
cd mcp-server && .venv/bin/python -c "from meshtastic_mcp import devices; print(devices.list_devices())"
```

Or via MCP: `mcp__meshtastic__list_devices()`

### Step 2 — Flash the device

```bash
# Via pio directly
pio run -e <env> -t upload --upload-port <port>

# Via MCP (any architecture)
# mcp__meshtastic__pio_flash(env="<env>", port="<port>", confirm=True)

# ESP32 factory flash (full erase)
# mcp__meshtastic__erase_and_flash(env="<env>", port="<port>", confirm=True)
```

**Capture flash output to a temp file:**

```bash
export MESHTASTIC_MCP_FLASH_LOG=/tmp/flash-<role>.log
: >"$MESHTASTIC_MCP_FLASH_LOG"
# ... run pio_flash / erase_and_flash ...
# Full pio/esptool output streams to that file in real-time
```

### Step 3 — Capture runtime logs

**Use `mcp-server/log-capture.sh`** — starts a long-running serial monitor and captures to a temp file:

```bash
# Start capture (auto-detect port for role)
./mcp-server/log-capture.sh --role esp32s3
# Output:
#   LOG_FILE=/tmp/meshtastic-log-abc123.txt
#   PID=12345
#   PORT=/dev/cu.usbmodem1101
#   ENV=heltec-v3

# With explicit port + env + optional flash
./mcp-server/log-capture.sh --port /dev/cu.X --env heltec-v3 [--flash heltec-v3]

# Poll the log file while device runs
tail -f /tmp/meshtastic-log-abc123.txt

# Stop capture when done
./mcp-server/log-capture.sh --stop 12345

# Read full log for analysis
cat /tmp/meshtastic-log-abc123.txt
```

**Via MCP tools directly:**

```python
# 1. Open serial monitor session
# mcp__meshtastic__serial_open(port="/dev/cu.X", env="heltec-v3")
# Returns: {session_id, resolved_baud, resolved_filters}

# 2. Poll for log lines (use cursor to page forward)
# mcp__meshtastic__serial_read(session_id="<id>", max_lines=200, since_cursor=0)
# Returns: {lines: [...], new_cursor: N, eof: bool, dropped: int}

# 3. Close when done
# mcp__meshtastic__serial_close(session_id="<id>")
```

| Tool                                    | Purpose                                                                    |
| --------------------------------------- | -------------------------------------------------------------------------- |
| `serial_open(port, env)`                | Start `pio device monitor` with board-specific filters + exception decoder |
| `serial_read(session_id, since_cursor)` | Read lines from 10k-line ring buffer; use cursor to page forward           |
| `serial_list()`                         | Find active sessions                                                       |
| `serial_close(session_id)`              | Stop monitor                                                               |
| `set_debug_log_api(enabled=True, port)` | Enable structured `meshtastic.log.line` protobuf stream                    |

### Step 4 — Analyze

Look for:

- `Guru Meditation`, `assert`, `panic` → firmware crash
- `E `, `Error` → error conditions
- `DEBUG|INFO|WARN` → log levels
- Boot sequence: `Boot...`, `I2C scan`, `initLoRa`, module startup lines
- Mesh events: `NODEINFO_APP`, `POSITION_APP`, `TX`/`RX` markers

Also check `mcp-server/tests/report.html` → `Meshtastic debug` section (200-line firmware log tail + device state dump on failure).

### Log file locations

| Log type         | Path                           | How it gets there                                  |
| ---------------- | ------------------------------ | -------------------------------------------------- |
| Flash subprocess | `/tmp/flash-<role>.log`        | `MESHTASTIC_MCP_FLASH_LOG` env var                 |
| Serial monitor   | `/tmp/meshtastic-log-<id>.txt` | `log-capture.sh` or serial session ring buffer     |
| Firmware pubsub  | `mcp-server/tests/fwlog.jsonl` | `_firmware_log_stream` fixture (test harness only) |

## Entry points

- **`src/main.cpp`**: `setup()` at line 308, `loop()` at line 1242
- **Boot sequence**: `waitUntilPowerLevelSafe()` → `earlyInitVariant()` [weak hook] → I2C scan → `setupModules()` → `initLoRa()` → `lateInitVariant()` [weak hook]
- **`userPrefs.jsonc`**: parsed at build time → `-D` compile flags (not runtime config)

## Architecture big-picture

Meshtastic firmware = C++17 embedded, target ESP32 / ESP32-S3/C3/C6, nRF52840, RP2040/RP2350, STM32WL, Linux-Portduino. One tree builds 200+ variants via PlatformIO envs + `variants/<arch>/<name>/{variant.h, platformio.ini}`.

- **Core mesh** (`src/mesh/`): `Router`, `NodeDB`, `Channels`, `CryptoEngine` (AES-CTR + X25519→AES-256-CCM), `StreamAPI`, `PhoneAPI`, `PacketAPI`, radio interfaces.
- **Modules** (`src/modules/`): `MeshModule` → `SinglePortModule` → `ProtobufModule<T>` hierarchy.
- **I2C auto-detection** (`src/detect/ScanI2C`): 80+ device types at boot.
- **MCP server** (`mcp-server/`): Python package, ~43 device-automation tools + tiered pytest suite. **StreamAPI `emitLogRecord`** uses dedicated `txBufLog`/`streamLock` — never break this.

## Encryption essentials

Two layers in `src/mesh/CryptoEngine.cpp`:

- **Channel** (AES-CTR): 1-byte PSK = short-form index, 16 bytes = AES-128, 32 bytes = AES-256.
- **Per-peer PKI** (X25519 ECDH → AES-256-CCM): used for DMs + remote admin.

`factory_reset_device` wipes the private key → DMs silently fail until NodeInfo re-exchange. `factory_reset_config` preserves it.

## House rules

- **No destructive device ops without operator approval.** `factory_reset`, `erase_and_flash`, `reboot`, `shutdown`, `uhubctl_cycle` — describe and stop. `confirm=True` is a real gate.
- **One MCP call per serial port at a time.** Sequence: `serial_open` → read/mutate → `serial_close`. Never parallelize on same port.
- **`userPrefs.jsonc` = session state during hardware tests.** Never edit from inside a test.
- **Run `trunk fmt` before commit.** `trunk_check` CI gate rejects unformatted code.
- **No speculate firmware root causes.** "Unknown" = valid classification — say so + list what would disambiguate.

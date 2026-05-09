# src/mesh/ — Core Mesh Networking

**Architectural center of the firmware.** Contains NodeDB (most-included header: 83 files), Router, CryptoEngine, Channels, and all radio interface implementations.

## Structure

```
mesh/
├── NodeDB.cpp/.h       # Node database (2308 lines, 83 includes)
├── Router.cpp/.h       # Packet routing (894 lines, 31 includes)
├── CryptoEngine.cpp/.h # AES-CTR + X25519→AES-256-CCM
├── Channels.cpp/.h     # Channel PSK management
├── MeshService.cpp/.h  # Mesh broadcast/unicast (52 includes)
├── PhoneAPI.cpp/.h    # Serial protocol (phone app)
├── StreamAPI.cpp/.h   # Serial StreamAPI (log record fix)
├── *Interface.cpp/.h  # Radio interfaces (SX126x, RF95, LR11x0, STM32WL)
├── api/                # WiFi/Ethernet ServerAPI
├── http/               # HTTP server
├── wifi/               # WiFi support
├── eth/                # Ethernet support
├── udp/                # UDP multicast
├── compression/        # unishox2 compression
└── generated/          # Protobuf generated code
```

## Key Files

| File                    | Lines | Role                                                    |
| ----------------------- | ----- | ------------------------------------------------------- |
| `NodeDB.cpp`            | 2,308 | Node database, device state persistence                 |
| `Router.cpp`            | 894   | Packet routing + encryption dispatch                    |
| `CryptoEngine.cpp`      | ~600  | AES-CTR (channels) + X25519→AES-256-CCM (PKI DMs/admin) |
| `RadioInterface.cpp`    | 1,176 | Base radio abstraction                                  |
| `RadioLibInterface.cpp` | 608   | RadioLib-based radio                                    |
| `MeshService.cpp`       | ~460  | Mesh broadcast/unicast                                  |

## Encryption (THIS PROJECT)

- **Channel**: AES-CTR, key = ChannelSettings.psk (0/1/16/32 bytes)
- **PKI DM**: X25519 ECDH → SHA-256 → AES-256-CCM, 12-byte overhead
- **Nonce**: packet_id‖from_node‖block_counter (channel) + extraNonce (PKI)
- Key gen: `generateKeyPair()` at boot if `!is_licensed && region != UNSET`
- See `.github/copilot-instructions.md` §Encryption for full spec.

## Critical Anti-Patterns

```
DO NOT:  Change state machine order in PhoneAPI.cpp:228
         Install defaults on nodeDB read failure (NodeDB.cpp:1264)
NEVER:   Send encrypted packets to boundChannel modules (MeshModule.cpp:128)
         Perform non-runOnce ops in NimBLE callbacks (NimbleBluetooth.cpp:421,472)
```

## Module Mixin Pattern

```cpp
class MyModule : public ProtobufModule<meshtastic_MyMessage>, private concurrency::OSThread
```

Modules inherit OSThread for periodic `runOnce()` tasks.

## Observer Pattern

```cpp
Observable<const meshtastic::Status *> newStatus;
CallbackObserver<MyClass, const meshtastic::Status *>(
    this, &MyClass::handleStatusUpdate);
```

## Concurrency

- `concurrency::Lock` + `LockGuard` for mutex (NodeDB, Router, CryptoEngine, PhoneAPI, StreamAPI)
- `SPILock` for radio SPI bus

## Protobuf

- Messages in `protobufs/meshtastic/*.proto`
- Generated code in `src/mesh/generated/meshtastic/`
- Regenerate: `bin/regen-protos.sh`

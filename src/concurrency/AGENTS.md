# src/concurrency/ — Threading Utilities

**Cross-cutting primitives used by ALL subsystems.** DO NOT modify without understanding the full impact.

## Files

| File                             | Pattern                       | Consumers                                                                       |
| -------------------------------- | ----------------------------- | ------------------------------------------------------------------------------- |
| `Lock.h/.cpp`                    | FreeRTOS mutex wrapper        | 18+ files: NodeDB, Router, CryptoEngine, PhoneAPI, StreamAPI, PowerFSM, ScanI2C |
| `LockGuard.h`                    | RAII lock guard               | Paired with Lock everywhere                                                     |
| `OSThread.h/.cpp`                | Periodic background task base | 9+ modules via mixin pattern                                                    |
| `NotifiedWorkerThread.h/.cpp`    | Event-driven worker           | Bluetooth, ServerAPI                                                            |
| `Periodic.h`                     | Recurring callback wrapper    | Various modules                                                                 |
| `InterruptableDelay.h/.cpp`      | Interruptible main-loop delay | main.cpp                                                                        |
| `BinarySemaphoreFreeRTOS.h/.cpp` | FreeRTOS binary semaphore     | InterruptableDelay on embedded                                                  |
| `BinarySemaphorePosix.h/.cpp`    | Posix binary semaphore        | InterruptableDelay on native/Portduino                                          |

## OSThread Mixin Pattern

```cpp
class MyModule : public ProtobufModule<meshtastic_MyMessage>, private concurrency::OSThread {
    int32_t runOnce() override { /* periodic work */ return 1000; } // next interval ms
};
```

Modules using this pattern: AudioModule, AtakPluginModule, CannedMessageModule, ExternalNotificationModule, NodeInfoModule, PositionModule, ReplyBotModule, TelemetryModule, TextMessageModule, TraceRouteModule, WaypointModule.

## Lock Usage

```cpp
concurrency::Lock myLock;
{
    concurrency::LockGuard guard(myLock);
    // protected section
}
```

## Thread Safety Rules

- **NodeDB**: node list, hash cache
- **Router**: packet queues, encryption state
- **CryptoEngine**: crypto operations
- **PhoneAPI/StreamAPI**: serial protocol buffers
- **PowerFSM**: power state machine
- **SPILock**: radio SPI bus (separate from general Lock)

## NotifiedWorkerThread

```cpp
class MyWorker : public NotifiedWorkerThread {
    void onNotify(uint32_t notify_arg) override;
};
// notify() / notifyFromISR() / notifyLater() wake the thread
```

## ISR Context Detection

```cpp
#define xPortInIsrContext() ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) == 0 ? pdFALSE : pdTRUE)
// Platform-specific implementations in each platform/ directory
```

## Observer Pattern (for events)

```cpp
Observable<const meshtastic::Status *> newStatus;
CallbackObserver<MyClass, const meshtastic::Status *>(
    this, &MyClass::handleStatusUpdate);
```

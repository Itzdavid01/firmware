#pragma once

#include "configuration.h"

#if defined(T_DECK_PRO_VOICE)

#include "MeshModule.h"
#include "concurrency/OSThread.h"

#ifdef ARCH_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

/**
 * Notification Manager for T-Deck Pro Voice.
 *
 * A MeshModule that intercepts incoming mesh packets and triggers
 * audio (I2S speaker) and haptic (vibration motor) feedback.
 *
 * IMPORTANT: All blocking playback (I2S writes, motor pulses) runs
 * in a dedicated FreeRTOS task, NOT in the main cooperative loop.
 * handleReceived() only enqueues a notification type and returns
 * immediately, preventing watchdog timeouts.
 */
class NotificationManager : public MeshModule, private concurrency::OSThread
{
  public:
    enum MessageType : uint8_t {
        MSG_NONE = 0,
        TEXT_MESSAGE,
        POSITION_UPDATE,
        ADMIN_MESSAGE,
        NODE_DISCOVERY,
        BROADCAST,
        BATTERY_LOW,
        ERROR_EVENT
    };

    NotificationManager();

    void setAudioEnabled(bool enabled) { audioEnabled = enabled; }
    bool isAudioEnabled() const { return audioEnabled; }

    void setHapticEnabled(bool enabled) { hapticEnabled = enabled; }
    bool isHapticEnabled() const { return hapticEnabled; }

    void setVolume(uint8_t volume);

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    bool audioEnabled;
    bool hapticEnabled;

    /// FreeRTOS queue for passing notification types to the playback task
    QueueHandle_t notifyQueue;

    /// The dedicated playback task handle
    TaskHandle_t playbackTaskHandle;

    /// Initialize speaker and motor hardware (called from the playback task).
    static void initHardware();

    /// The FreeRTOS task function that does blocking audio/haptic playback.
    static void playbackTask(void *param);

    /// Perform the actual (blocking) notification for a message type.
    static void doNotify(MessageType type, bool audioOn, bool hapticOn);

    MessageType classifyPacket(const meshtastic_MeshPacket &mp);
};

extern NotificationManager *notificationManager;

#endif // T_DECK_PRO_VOICE

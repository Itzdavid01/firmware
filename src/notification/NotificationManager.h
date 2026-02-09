#pragma once

#include "configuration.h"

#if defined(T_DECK_PRO_VOICE)

#include "MeshModule.h"
#include "concurrency/OSThread.h"

/**
 * Notification Manager for T-Deck Pro Voice.
 *
 * A MeshModule that listens for incoming mesh packets and triggers
 * audio (I2S speaker) and haptic (vibration motor) notifications
 * based on message type.
 *
 * Different message types produce different feedback patterns:
 *   - Text messages:      short beep + short vibration
 *   - Position updates:   double beep (no vibration, too frequent)
 *   - Admin messages:     ascending beep + double vibration
 *   - Node discovery:     ascending beep + double vibration
 *   - Battery low:        warning tone + long vibration
 */
class NotificationManager : public MeshModule, private concurrency::OSThread
{
  public:
    enum MessageType {
        TEXT_MESSAGE,
        POSITION_UPDATE,
        ADMIN_MESSAGE,
        NODE_DISCOVERY,
        BROADCAST,
        BATTERY_LOW,
        ERROR_EVENT
    };

    NotificationManager();

    /// Trigger a notification for a given message type.
    void notify(MessageType type);

    /// Enable/disable audio notifications.
    void setAudioEnabled(bool enabled) { audioEnabled = enabled; }
    bool isAudioEnabled() const { return audioEnabled; }

    /// Enable/disable haptic notifications.
    void setHapticEnabled(bool enabled) { hapticEnabled = enabled; }
    bool isHapticEnabled() const { return hapticEnabled; }

    /// Set speaker volume (0-100).
    void setVolume(uint8_t volume);

  protected:
    virtual bool wantPacket(const meshtastic_MeshPacket *p) override;
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
    virtual int32_t runOnce() override;

  private:
    bool audioEnabled;
    bool hapticEnabled;
    bool hardwareInitialized;

    /// Initialize speaker and motor hardware on first use.
    void initHardware();

    /// Determine the message type from a mesh packet.
    MessageType classifyPacket(const meshtastic_MeshPacket &mp);
};

extern NotificationManager *notificationManager;

#endif // T_DECK_PRO_VOICE

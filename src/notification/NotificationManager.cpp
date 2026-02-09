#include "configuration.h"

#if defined(T_DECK_PRO_VOICE)

#include "NotificationManager.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "Router.h"
#include "main.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
#include "audio/I2SSpeaker.h"
#endif

#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
#include "haptic/VibrationMotor.h"
#endif

NotificationManager *notificationManager = nullptr;

NotificationManager::NotificationManager()
    : MeshModule("NotificationManager"), concurrency::OSThread("NotificationMgr"), audioEnabled(true), hapticEnabled(true),
      hardwareInitialized(false)
{
    // We want to see all decoded packets, not just a single portnum
    isPromiscuous = true;

    LOG_INFO("NotificationManager: created (audio=%s, haptic=%s)", audioEnabled ? "on" : "off", hapticEnabled ? "on" : "off");
}

void NotificationManager::initHardware()
{
    if (hardwareInitialized) {
        return;
    }

#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
    if (!i2sSpeaker) {
        i2sSpeaker = new I2SSpeaker();
    }
    if (!i2sSpeaker->init()) {
        LOG_ERROR("NotificationManager: failed to init I2S speaker");
    }
#endif

#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
    if (!vibrationMotor) {
        vibrationMotor = new VibrationMotor();
    }
    if (!vibrationMotor->init()) {
        LOG_ERROR("NotificationManager: failed to init vibration motor");
    }
#endif

    hardwareInitialized = true;
    LOG_INFO("NotificationManager: hardware initialized");
}

bool NotificationManager::wantPacket(const meshtastic_MeshPacket *p)
{
    // Accept decoded packets that are not from us
    if (p->which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
        switch (p->decoded.portnum) {
        case meshtastic_PortNum_TEXT_MESSAGE_APP:
        case meshtastic_PortNum_TEXT_MESSAGE_COMPRESSED_APP:
        case meshtastic_PortNum_POSITION_APP:
        case meshtastic_PortNum_NODEINFO_APP:
        case meshtastic_PortNum_ADMIN_APP:
            return true;
        default:
            return false;
        }
    }
    return false;
}

ProcessMessage NotificationManager::handleReceived(const meshtastic_MeshPacket &mp)
{
    // Don't notify for our own messages
    if (isFromUs(&mp)) {
        return ProcessMessage::CONTINUE;
    }

    // Initialize hardware on first received message (lazy init)
    if (!hardwareInitialized) {
        initHardware();
    }

    MessageType type = classifyPacket(mp);
    notify(type);

    // Always continue so other modules can also process the packet
    return ProcessMessage::CONTINUE;
}

NotificationManager::MessageType NotificationManager::classifyPacket(const meshtastic_MeshPacket &mp)
{
    switch (mp.decoded.portnum) {
    case meshtastic_PortNum_TEXT_MESSAGE_APP:
    case meshtastic_PortNum_TEXT_MESSAGE_COMPRESSED_APP:
        return TEXT_MESSAGE;

    case meshtastic_PortNum_POSITION_APP:
        return POSITION_UPDATE;

    case meshtastic_PortNum_NODEINFO_APP:
        return NODE_DISCOVERY;

    case meshtastic_PortNum_ADMIN_APP:
        return ADMIN_MESSAGE;

    default:
        return BROADCAST;
    }
}

void NotificationManager::notify(MessageType type)
{
    switch (type) {
    case TEXT_MESSAGE:
        LOG_INFO("NotificationManager: text message notification");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioEnabled && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_SHORT);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticEnabled && vibrationMotor) {
            vibrationMotor->pulse(50, 180);
        }
#endif
        break;

    case POSITION_UPDATE:
        // Position updates are frequent; only audio feedback, no vibration
        LOG_DEBUG("NotificationManager: position update notification");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioEnabled && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_DOUBLE);
        }
#endif
        break;

    case NODE_DISCOVERY:
        LOG_INFO("NotificationManager: node discovery notification");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioEnabled && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_ASCENDING);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticEnabled && vibrationMotor) {
            vibrationMotor->pattern(VibrationMotor::HAPTIC_DOUBLE);
        }
#endif
        break;

    case ADMIN_MESSAGE:
        LOG_INFO("NotificationManager: admin message notification");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioEnabled && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_ASCENDING);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticEnabled && vibrationMotor) {
            vibrationMotor->pattern(VibrationMotor::HAPTIC_DOUBLE);
        }
#endif
        break;

    case BATTERY_LOW:
        LOG_WARN("NotificationManager: battery low notification");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioEnabled && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_WARNING);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticEnabled && vibrationMotor) {
            vibrationMotor->pattern(VibrationMotor::HAPTIC_LONG);
        }
#endif
        break;

    case ERROR_EVENT:
        LOG_ERROR("NotificationManager: error notification");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioEnabled && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_WARNING);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticEnabled && vibrationMotor) {
            vibrationMotor->pattern(VibrationMotor::HAPTIC_TRIPLE);
        }
#endif
        break;

    case BROADCAST:
    default:
        LOG_DEBUG("NotificationManager: generic broadcast notification");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioEnabled && i2sSpeaker) {
            i2sSpeaker->playTone(800, 80);
        }
#endif
        break;
    }
}

void NotificationManager::setVolume(uint8_t volume)
{
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
    if (i2sSpeaker) {
        i2sSpeaker->setVolume(volume);
    }
#endif
}

int32_t NotificationManager::runOnce()
{
    // The notification manager doesn't need periodic processing;
    // notifications are triggered via handleReceived().
    // Return max interval to minimize CPU usage.
    return INT32_MAX;
}

#endif // T_DECK_PRO_VOICE

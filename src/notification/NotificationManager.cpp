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

// Struct passed through the FreeRTOS queue
struct NotifyEvent {
    NotificationManager::MessageType type;
    bool audioOn;
    bool hapticOn;
};

// Flag to track if hardware has been initialized (done once in the playback task)
static bool hwInitDone = false;

void NotificationManager::initHardware()
{
    if (hwInitDone) {
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

    hwInitDone = true;
    LOG_INFO("NotificationManager: hardware initialized in playback task");
}

void NotificationManager::playbackTask(void *param)
{
    QueueHandle_t queue = (QueueHandle_t)param;

    // Initialize hardware in this task's context (safe for blocking I2S driver install)
    initHardware();

    NotifyEvent event;
    while (true) {
        // Block until a notification arrives
        if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
            doNotify(event.type, event.audioOn, event.hapticOn);
        }
    }
}

void NotificationManager::doNotify(MessageType type, bool audioOn, bool hapticOn)
{
    switch (type) {
    case TEXT_MESSAGE:
        LOG_INFO("Notification: text message");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioOn && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_SHORT);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticOn && vibrationMotor) {
            vibrationMotor->pulse(50, 180);
        }
#endif
        break;

    case POSITION_UPDATE:
        // Position updates are frequent - audio only, no vibration
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioOn && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_DOUBLE);
        }
#endif
        break;

    case NODE_DISCOVERY:
        LOG_INFO("Notification: node discovery");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioOn && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_ASCENDING);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticOn && vibrationMotor) {
            vibrationMotor->pattern(VibrationMotor::HAPTIC_DOUBLE);
        }
#endif
        break;

    case ADMIN_MESSAGE:
        LOG_INFO("Notification: admin message");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioOn && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_ASCENDING);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticOn && vibrationMotor) {
            vibrationMotor->pattern(VibrationMotor::HAPTIC_DOUBLE);
        }
#endif
        break;

    case BATTERY_LOW:
        LOG_WARN("Notification: battery low");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioOn && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_WARNING);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticOn && vibrationMotor) {
            vibrationMotor->pattern(VibrationMotor::HAPTIC_LONG);
        }
#endif
        break;

    case ERROR_EVENT:
        LOG_ERROR("Notification: error");
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioOn && i2sSpeaker) {
            i2sSpeaker->playBeep(I2SSpeaker::BEEP_WARNING);
        }
#endif
#if defined(HAS_VIBRATION_MOTOR) && defined(ARCH_ESP32)
        if (hapticOn && vibrationMotor) {
            vibrationMotor->pattern(VibrationMotor::HAPTIC_TRIPLE);
        }
#endif
        break;

    case BROADCAST:
    default:
#if defined(HAS_I2S_SPEAKER) && defined(ARCH_ESP32)
        if (audioOn && i2sSpeaker) {
            i2sSpeaker->playTone(800, 80);
        }
#endif
        break;

    case MSG_NONE:
        break;
    }
}

NotificationManager::NotificationManager()
    : MeshModule("NotificationManager"), concurrency::OSThread("NotificationMgr"), audioEnabled(true), hapticEnabled(true),
      notifyQueue(nullptr), playbackTaskHandle(nullptr)
{
    isPromiscuous = true;

    // Create a FreeRTOS queue for notification events (depth 4 - drop old if full)
    notifyQueue = xQueueCreate(4, sizeof(NotifyEvent));
    if (!notifyQueue) {
        LOG_ERROR("NotificationManager: failed to create notification queue");
        return;
    }

    // Create a dedicated FreeRTOS task for blocking audio/haptic playback.
    // Stack size 4096 is sufficient for sine wave generation + I2S writes.
    BaseType_t ret =
        xTaskCreate(playbackTask, "notify_play", 4096, (void *)notifyQueue, 1, // Low priority
                    &playbackTaskHandle);
    if (ret != pdPASS) {
        LOG_ERROR("NotificationManager: failed to create playback task");
        vQueueDelete(notifyQueue);
        notifyQueue = nullptr;
        return;
    }

    LOG_INFO("NotificationManager: created with dedicated playback task");
}

bool NotificationManager::wantPacket(const meshtastic_MeshPacket *p)
{
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
    if (isFromUs(&mp)) {
        return ProcessMessage::CONTINUE;
    }

    if (!notifyQueue) {
        return ProcessMessage::CONTINUE;
    }

    // Classify and enqueue - this returns IMMEDIATELY, no blocking
    NotifyEvent event;
    event.type = classifyPacket(mp);
    event.audioOn = audioEnabled;
    event.hapticOn = hapticEnabled;

    // Non-blocking send: if queue is full, the notification is dropped (acceptable)
    xQueueSend(notifyQueue, &event, 0);

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
    // All work is done in the dedicated playback task.
    // This OSThread does nothing - return max interval.
    return INT32_MAX;
}

#endif // T_DECK_PRO_VOICE

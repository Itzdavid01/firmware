#if defined(T_DECK_PRO)

#include "TDeckProKeyboard.h"
#include "main.h"
#include "buzz.h"

#define _TCA8418_COLS 10
#define _TCA8418_ROWS 4
#define _TCA8418_NUM_KEYS 35

#define _TCA8418_MULTI_TAP_THRESHOLD 1500

using Key = TCA8418KeyboardBase::TCA8418Key;

constexpr uint8_t modifierRightShiftKey = 31 - 1; // keynum -1
constexpr uint8_t modifierRightShift = 0b0001;
constexpr uint8_t modifierLeftShiftKey = 35 - 1;
constexpr uint8_t modifierLeftShift = 0b0001;
constexpr uint8_t modifierSymKey = 32 - 1;
constexpr uint8_t modifierSym = 0b0010;
constexpr uint8_t modifierAltKey = 30 - 1;
constexpr uint8_t modifierAlt = 0b0100;

// Num chars per key, Modulus for rotating through characters
static uint8_t TDeckProTapMod[_TCA8418_NUM_KEYS] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                                    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

static unsigned char TDeckProTapMap[_TCA8418_NUM_KEYS][5] = {
    {'p', 'P', '@', 0x00, Key::SEND_PING},
    {'o', 'O', '+'},
    {'i', 'I', '-'},
    {'u', 'U', '_'},
    {'y', 'Y', ')'},
    {'t', 'T', '(', 0x00, Key::TAB},
    {'r', 'R', '3'},
    {'e', 'E', '2', 0x00, Key::UP},
    {'w', 'W', '1'},
    {'q', 'Q', '#', 0x00, Key::ESC}, // p, o, i, u, y, t, r, e, w, q
    {Key::BSP, 0x00, 0x00},
    {'l', 'L', '"'},
    {'k', 'K', '\''},
    {'j', 'J', ';'},
    {'h', 'H', ':'},
    {'g', 'G', '/', 0x00, Key::GPS_TOGGLE},
    {'f', 'F', '6', 0x00, Key::RIGHT},
    {'d', 'D', '5'},
    {'s', 'S', '4', 0x00, Key::LEFT},
    {'a', 'A', '*'}, // bsp, l, k, j, h, g, f, d, s, a
    {0x0d, 0x00, 0x00},
    {'$', Key::READ_ALOUD, 0x00, 0x00, Key::TOUCH_LOCK},  // SPEAKER key: tap='$', shift=read aloud, alt=touch lock toggle
    {'m', 'M', '.', 0x00, Key::MUTE_TOGGLE},
    {'n', 'N', ','},
    {'b', 'B', '!', 0x00, Key::BL_TOGGLE},
    {'v', 'V', '?'},
    {'c', 'C', '9'},
    {'x', 'X', '8', 0x00, Key::DOWN},
    {'z', 'Z', '7'},
    {0x00, 0x00, 0x00}, // Ent, $, m, n, b, v, c, x, z, alt
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x20, 0x00, 0x00},
    {0x00, 0x00, '0'},
    {0x00, 0x00, 0x00} // R_Shift, sym, space, mic, L_Shift
};

TDeckProKeyboard::TDeckProKeyboard()
    : TCA8418KeyboardBase(_TCA8418_ROWS, _TCA8418_COLS), modifierFlag(0), last_modifier_time(0), last_key(UINT8_MAX),
      next_key(UINT8_MAX), last_tap(0L), char_idx(0), tap_interval(0), _bl_on(false)
{
}

void TDeckProKeyboard::reset()
{
    TCA8418KeyboardBase::reset();
    pinMode(KB_BL_PIN, OUTPUT);
    setBacklight(false);
}

// handle multi-key presses (shift and alt)
void TDeckProKeyboard::trigger()
{
#if defined(HAS_DRV2605)
    uint32_t now = millis();
    if (_vibration_end != 0 && now >= _vibration_end) {
        if (_haptic_phase == 0) {
            drv.stop();
            _vibration_end = 0;
        } else if (_haptic_phase == 1) {
            drv.stop();
            _haptic_phase = 2;
            _vibration_end = now + 30;
        } else if (_haptic_phase == 2) {
            drv.go();
            _haptic_phase = 3;
            _vibration_end = now + 80;
        } else {
            drv.stop();
            _haptic_phase = 0;
            _vibration_end = 0;
        }
    }
#endif

    uint8_t count = keyCount();
    if (count == 0)
        return;
    for (uint8_t i = 0; i < count; ++i) {
        uint8_t k = readRegister(TCA8418_REG_KEY_EVENT_A + i);
        uint8_t key = k & 0x7F;
        if (k & 0x80) {
            pressed(key);
        } else {
            released();
            state = Idle;
        }
    }
}

void TDeckProKeyboard::pressed(uint8_t key)
{
    if (state == Init || state == Busy) {
        return;
    }
    LOG_INFO("TDeckProKeyboard: keypress detected, triggering feedback");
    hapticFeedback(false);
    playClick();
    int row = (key - 1) / 10;
    int col = (key - 1) % 10;
    LOG_DEBUG("TDeckPro: raw key=%u row=%d col=%d", key, row, col);

    if (modifierFlag && (millis() - last_modifier_time > _TCA8418_MULTI_TAP_THRESHOLD)) {
        modifierFlag = 0;
    }

    if (row >= _TCA8418_ROWS || col >= _TCA8418_COLS) {
        return; // Invalid key
    }

    next_key = row * _TCA8418_COLS + col;
    LOG_DEBUG("TDeckPro: next_key=%u (row=%d col=%d cols=%d)", next_key, row, col, _TCA8418_COLS);
    state = Held;

    uint32_t now = millis();
    tap_interval = now - last_tap;

    updateModifierFlag(next_key);
    if (isModifierKey(next_key)) {
        last_modifier_time = now;
    }

    if (tap_interval < 0) {
        last_tap = 0;
        state = Busy;
        return;
    }

    if (next_key != last_key || tap_interval > _TCA8418_MULTI_TAP_THRESHOLD) {
        char_idx = 0;
    } else {
        char_idx += 1;
    }

    last_key = next_key;
    last_tap = now;
}

void TDeckProKeyboard::released()
{
    if (state != Held) {
        return;
    }

    if (last_key >= _TCA8418_NUM_KEYS) {
        last_key = UINT8_MAX;
        state = Idle;
        return;
    }

    uint32_t now = millis();
    last_tap = now;

    uint8_t mapIdx = modifierFlag % TDeckProTapMod[last_key];
    uint8_t action = TDeckProTapMap[last_key][mapIdx];
    LOG_DEBUG("TDeckPro: released last_key=%u modifierFlag=%u mapIdx=%u action=0x%02x", last_key, modifierFlag, mapIdx, action);

    if (action == Key::BL_TOGGLE) {
        LOG_INFO("TDeckPro: toggling backlight");
        toggleBacklight();
        return;
    }

    queueEvent(action);
    if (isModifierKey(last_key) == false)
        modifierFlag = 0;
}

void TDeckProKeyboard::setBacklight(bool on)
{
    _bl_on = on;
    if (on) {
        digitalWrite(KB_BL_PIN, HIGH);
    } else {
        digitalWrite(KB_BL_PIN, LOW);
    }
}

void TDeckProKeyboard::toggleBacklight(void)
{
    setBacklight(!_bl_on);
}

void TDeckProKeyboard::hapticFeedback(bool special)
{
#if defined(HAS_DRV2605)
    _vibration_end = millis() + (special ? 160 : 120);
    _haptic_phase = 1;
    drv.setWaveform(0, 16);
    drv.setWaveform(1, 0);
    drv.setWaveform(2, 16);
    drv.setWaveform(3, 0);
    drv.setWaveform(4, 0);
    drv.go();
#endif
}

void TDeckProKeyboard::updateModifierFlag(uint8_t key)
{
    if (key == modifierRightShiftKey) {
        modifierFlag ^= modifierRightShift;
        LOG_DEBUG("TDeckPro: RShift modifierFlag=%u", modifierFlag);
    } else if (key == modifierLeftShiftKey) {
        modifierFlag ^= modifierLeftShift;
        LOG_DEBUG("TDeckPro: LShift modifierFlag=%u", modifierFlag);
    } else if (key == modifierSymKey) {
        modifierFlag ^= modifierSym;
        LOG_DEBUG("TDeckPro: Sym modifierFlag=%u", modifierFlag);
    } else if (key == modifierAltKey) {
        modifierFlag ^= modifierAlt;
        LOG_DEBUG("TDeckPro: Alt modifierFlag=%u", modifierFlag);
    }
}

bool TDeckProKeyboard::isModifierKey(uint8_t key)
{
    return (key == modifierRightShiftKey || key == modifierLeftShiftKey || key == modifierAltKey || key == modifierSymKey);
}

#endif // T_DECK_PRO
// Adapter that turns CST328 touch events into InkHUD nav events.
// First cut: three vertical zones (left/center/right) -> navLeft / shortpress / navRight on touch-up.
// Long-press (>= 800 ms) -> openMenu via longpress().

#pragma once

#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "concurrency/OSThread.h"
#include <stdint.h>

namespace TDPV
{

class CST328InkHUDAdapter : public concurrency::OSThread
{
  public:
    CST328InkHUDAdapter();
    void begin();

  protected:
    int32_t runOnce() override;

  private:
    bool started = false;
    bool wasDown = false;
    int16_t downX = 0;
    int16_t downY = 0;
    uint32_t downAtMs = 0;
    bool longFired = false;
};

} // namespace TDPV

#endif // MESHTASTIC_INCLUDE_INKHUD

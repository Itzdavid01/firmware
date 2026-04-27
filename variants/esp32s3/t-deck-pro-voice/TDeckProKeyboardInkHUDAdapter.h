// Adapter that drives TDeckProKeyboard (TCA8418 matrix) directly into InkHUD,
// bypassing the standard InputBroker (which is excluded under MESHTASTIC_INCLUDE_INKHUD).

#pragma once

#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "concurrency/OSThread.h"

class TDeckProKeyboard;

namespace TDPV
{

class TDeckProKeyboardInkHUDAdapter : public concurrency::OSThread
{
  public:
    TDeckProKeyboardInkHUDAdapter();
    void begin();

  protected:
    int32_t runOnce() override;

  private:
    void dispatch(char c);

    TDeckProKeyboard *kb = nullptr;
    bool started = false;
};

} // namespace TDPV

#endif // MESHTASTIC_INCLUDE_INKHUD

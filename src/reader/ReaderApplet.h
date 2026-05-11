#ifdef MESHTASTIC_INCLUDE_INKHUD

#pragma once
#include "graphics/niche/InkHUD/Applet.h"

namespace reader
{

// InkHUD Applet wrapping the ReaderController.
// All drawing goes through Applet::drawPixel() -> Tile -> Renderer -> ED047TC1Parallel.
class ReaderApplet : public NicheGraphics::InkHUD::Applet
{
  public:
    ReaderApplet();

    // -- Applet lifecycle ----------------------------------
    void onActivate() override;
    void onDeactivate() override;
    void onForeground() override;
    void onBackground() override;
    void onRender(bool full) override;

    // -- Input handlers ------------------------------------
    void onButtonShortPress() override; // SELECT -> open book / advance page
    void onButtonLongPress() override;  // BACK  -> return to book list
    void onNavDown() override;          // scroll list down / next page
    void onNavUp() override;            // scroll list up / prev page

  private:
    enum class View { BOOK_LIST, READING };
    View currentView = View::BOOK_LIST;

    // Debounce progress saves (2s)
    uint32_t lastSaveMs = 0;
    static constexpr uint32_t PROGRESS_SAVE_DEBOUNCE_MS = 2000;

    void refreshBookList();
    void refreshReading();
    void saveProgressDebounced();

    // Input subscription bitmask
    static constexpr uint8_t INPUTS = BUTTON_SHORT | BUTTON_LONG | NAV_UP | NAV_DOWN;
};

} // namespace reader

#endif

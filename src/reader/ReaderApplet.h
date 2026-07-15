#ifdef MESHTASTIC_INCLUDE_INKHUD

#pragma once
#include "graphics/niche/InkHUD/Applet.h"
#include <cstdint>
#include <string>
#include <vector>

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

    // -- Typography / page layout --------------------------
    // OpenDyslexic body font (built lazily so static AppletFont init order is safe).
    NicheGraphics::InkHUD::AppletFont fontBody;
    bool fontBodyReady = false;
    void ensureBodyFont();

    // One greedily-packed line of body text.
    struct Line {
        size_t start = 0;          // first char of the line (inclusive)
        size_t contentEnd = 0;     // one past the last drawn char (exclusive)
        size_t nextStart = 0;      // where the following line begins
        uint16_t naturalWidth = 0; // px width of words + single base spaces
        int wordCount = 0;
        bool paragraphEnd = false; // line ended on '\n' or end-of-text -> don't justify
    };

    // Body geometry, recomputed each render/measure pass (cheap).
    int16_t bodyLeft = 0;
    int16_t bodyTop = 0;
    uint16_t bodyWidth = 0;
    int16_t bodyBottom = 0;
    int16_t linePitch = 0;
    int linesPerPage = 1;
    void computeBodyGeometry(); // sets fontBody + the fields above

    Line layoutLine(const std::string &text, size_t offset, uint16_t maxW);
    void drawLine(const std::string &text, const Line &line, int16_t top, uint16_t maxW);
    // Lay out one page from `start`; draw it when `draw`. Returns next page start.
    size_t renderPage(const std::string &text, size_t start, bool draw);

    // Page boundaries (char offsets) for the chapter currently paginated.
    std::vector<int32_t> pageStarts;
    int pageIdx = 0;
    int paginatedChapter = -1;
    std::string paginatedBook;
    void ensurePagination();                     // (re)build pageStarts when chapter/book changes; map saved offset
    void paginateToEnd(const std::string &text); // fill pageStarts for the whole chapter
    void readerNextPage();
    void readerPrevPage();
    void syncControllerOffset(); // push pageStarts[pageIdx] into the controller

    // Layout tuning (px)
    static constexpr int16_t MARGIN_X = 28;
    static constexpr int16_t MARGIN_TOP = 16;
    static constexpr int16_t MARGIN_BOTTOM = 24;
    static constexpr int16_t LEADING = 6;

    // Input subscription bitmask
    static constexpr uint8_t INPUTS = BUTTON_SHORT | BUTTON_LONG | NAV_UP | NAV_DOWN;
};

} // namespace reader

#endif

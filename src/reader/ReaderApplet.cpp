#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "ReaderApplet.h"
#include "RTC.h"
#include "ReaderFSM.h"

using namespace NicheGraphics;
using namespace NicheGraphics::InkHUD;
using namespace NicheGraphics::Drivers;

namespace reader
{

static constexpr int BEZEL_MARGIN = 24;

ReaderApplet::ReaderApplet()
{
    name = "Reader";
    setInputsSubscribed(INPUTS, true);
}

void ReaderApplet::onActivate()
{
    ReaderController::instance().scanForBooks();
}

void ReaderApplet::onDeactivate()
{
    if (ReaderController::instance().isBookOpen()) {
        auto p = ReaderController::instance().getProgress();
        ReaderController::instance().setProgress(p);
    }
}

void ReaderApplet::onForeground()
{
    requestUpdate(Drivers::EInk::UpdateTypes::FAST, true);
}

void ReaderApplet::onBackground()
{
    if (ReaderController::instance().isBookOpen()) {
        auto p = ReaderController::instance().getProgress();
        ReaderController::instance().setProgress(p);
    }
}

void ReaderApplet::onRender(bool /*full*/)
{
    resetDrawingSpace();
    switch (currentView) {
    case View::BOOK_LIST:
        refreshBookList();
        break;
    case View::READING:
        refreshReading();
        break;
    }
}

void ReaderApplet::onButtonShortPress()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::BOOK_LIST) {
        if (!ctrl.getBookFiles().empty()) {
            if (ctrl.openBook(ctrl.getSelectedBookIndex())) {
                currentView = View::READING;
                requestUpdate(Drivers::EInk::UpdateTypes::FAST, true);
            }
        }
    } else {
        ctrl.nextPage();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
        saveProgressDebounced();
    }
}

void ReaderApplet::onButtonLongPress()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::READING) {
        ctrl.closeBook();
        currentView = View::BOOK_LIST;
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, true);
    } else {
        ctrl.closeBook();
        sendToBackground();
        inkhud->prevApplet();
    }
}

void ReaderApplet::onNavDown()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::BOOK_LIST) {
        ctrl.selectNextBook();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
    } else {
        ctrl.nextPage();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
        saveProgressDebounced();
    }
}

void ReaderApplet::onNavUp()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::BOOK_LIST) {
        ctrl.selectPrevBook();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
    } else {
        ctrl.prevPage();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
        saveProgressDebounced();
    }
}

void ReaderApplet::refreshBookList()
{
    fillRect(0, 0, width(), height(), Color::WHITE);
    // Offset header by bezel margin
    int headerY = BEZEL_MARGIN;
    drawHeader("E-Reader", headerY);

    auto &books = ReaderController::instance().getBookFiles();
    int selected = ReaderController::instance().getSelectedBookIndex();
    int startY = headerY + getHeaderHeight() + 8;

    if (books.empty()) {
        printAt(BEZEL_MARGIN + 4, Y(0.3f), "No books found.");
        printAt(BEZEL_MARGIN + 4, Y(0.4f), "Copy .epub to /sd/");
        return;
    }

    for (int i = 0; i < (int)books.size() && i < 12; i++) { // Show more items on big screen
        bool sel = (i == selected);
        int16_t y = startY + i * (fontMedium.lineHeight() + 8);
        if (sel) {
            fillRect(BEZEL_MARGIN, y, width() - 2 * BEZEL_MARGIN, fontMedium.lineHeight() + 8, Color::BLACK);
            setTextColor(Color::WHITE);
        } else {
            setTextColor(Color::BLACK);
        }
        std::string path = books[i];
        size_t slash = path.find_last_of('/');
        std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
        printAt(BEZEL_MARGIN + 8, y + 2, name);
    }
    setTextColor(Color::BLACK);
}

void ReaderApplet::refreshReading()
{
    auto &ctrl = ReaderController::instance();
    fillRect(0, 0, width(), height(), Color::WHITE);

    std::string bookName = ctrl.getOpenBookPath();
    size_t slash = bookName.find_last_of('/');
    if (slash != std::string::npos)
        bookName = bookName.substr(slash + 1);
    char header[64];
    snprintf(header, sizeof(header), "%s %d/%d", bookName.c_str(), ctrl.getCurrentChapter() + 1, ctrl.getChapterCount());

    int headerY = BEZEL_MARGIN;
    drawHeader(header, headerY);

    const std::string &text = ctrl.getCurrentChapterText();
    int offset = ctrl.getCurrentPageOffset();

    if (offset >= 0 && offset < (int)text.size()) {
        int lineStart = offset;
        while (lineStart > 0 && text[lineStart - 1] != ' ' && text[lineStart - 1] != '\n')
            lineStart--;
        std::string displayText = text.substr(lineStart);
        int16_t top = headerY + getHeaderHeight() + 12;
        uint16_t maxW = width() - 2 * BEZEL_MARGIN - 8;
        printWrapped(BEZEL_MARGIN + 4, top, maxW, displayText);
    } else if (text.empty()) {
        printAt(BEZEL_MARGIN + 4, Y(0.3f), "[empty chapter]");
    }
}

void ReaderApplet::saveProgressDebounced()
{
    uint32_t now = millis();
    if (now - lastSaveMs >= PROGRESS_SAVE_DEBOUNCE_MS) {
        lastSaveMs = now;
        auto p = ReaderController::instance().getProgress();
        ReaderController::instance().setProgress(p);
    }
}

} // namespace reader

#endif

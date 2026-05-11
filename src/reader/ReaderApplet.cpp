#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "ReaderApplet.h"
#include "RTC.h"
#include "ReaderFSM.h"

using namespace NicheGraphics;

namespace reader
{

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
    fillRect(0, 0, width(), height(), WHITE);
    drawHeader("E-Reader");

    auto &books = ReaderController::instance().getBookFiles();
    int selected = ReaderController::instance().getSelectedBookIndex();
    int startY = getHeaderHeight() + 8;

    if (books.empty()) {
        printAt(X(0.05f), Y(0.3f), "No books found.");
        printAt(X(0.05f), Y(0.4f), "Copy .epub to /sd/");
        return;
    }

    for (int i = 0; i < (int)books.size() && i < 8; i++) {
        bool sel = (i == selected);
        int16_t y = startY + i * (fontMedium.lineHeight() + 4);
        if (sel) {
            fillRect(0, y, width(), fontMedium.lineHeight() + 4, BLACK);
            setTextColor(WHITE);
        } else {
            setTextColor(BLACK);
        }
        std::string path = books[i];
        size_t slash = path.find_last_of('/');
        std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
        printAt(4, y, name);
    }
    setTextColor(BLACK);
}

void ReaderApplet::refreshReading()
{
    auto &ctrl = ReaderController::instance();
    fillRect(0, 0, width(), height(), WHITE);

    std::string bookName = ctrl.getOpenBookPath();
    size_t slash = bookName.find_last_of('/');
    if (slash != std::string::npos)
        bookName = bookName.substr(slash + 1);
    char header[64];
    snprintf(header, sizeof(header), "%s %d/%d", bookName.c_str(), ctrl.getCurrentChapter() + 1, ctrl.getChapterCount());
    drawHeader(header);

    const std::string &text = ctrl.getCurrentChapterText();
    int offset = ctrl.getCurrentPageOffset();

    if (offset > 0 && offset < (int)text.size()) {
        int lineStart = offset;
        while (lineStart > 0 && text[lineStart - 1] != ' ' && text[lineStart - 1] != '\n')
            lineStart--;
        std::string displayText = text.substr(lineStart);
        int16_t top = getHeaderHeight() + 6;
        uint16_t maxW = width() - 8;
        printWrapped(4, top, maxW, displayText);
    } else if (text.empty()) {
        printAt(X(0.05f), Y(0.3f), "[empty chapter]");
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

#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "ReaderApplet.h"
#include "RTC.h"
#include "ReaderFSM.h"
#include "graphics/niche/Fonts/OpenDyslexic14pt.h"
#include <algorithm>
#include <cstdio>
#include <vector>

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
                paginatedChapter = -1; // force fresh pagination for the new book
                requestUpdate(Drivers::EInk::UpdateTypes::FAST, true);
            }
        }
    } else {
        readerNextPage();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
        saveProgressDebounced();
    }
}

void ReaderApplet::onButtonLongPress()
{
    auto &ctrl = ReaderController::instance();
    if (currentView == View::READING) {
        ctrl.closeBook();
        paginatedChapter = -1;
        pageStarts.clear();
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
        readerNextPage();
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
        readerPrevPage();
        requestUpdate(Drivers::EInk::UpdateTypes::FAST, false);
        saveProgressDebounced();
    }
}

void ReaderApplet::refreshBookList()
{
    fillRect(0, 0, width(), height(), Color::WHITE);
    setFont(fontMedium);
    int headerY = BEZEL_MARGIN;
    drawHeader("E-Reader");

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

    // Header (use the small UI font so it stays inside the header band).
    setFont(fontSmall);
    std::string bookName = ctrl.getOpenBookPath();
    size_t slash = bookName.find_last_of('/');
    if (slash != std::string::npos)
        bookName = bookName.substr(slash + 1);

    ensurePagination();
    char header[80];
    snprintf(header, sizeof(header), "%s  %d/%d  p.%d", bookName.c_str(), ctrl.getCurrentChapter() + 1, ctrl.getChapterCount(),
             pageIdx + 1);
    drawHeader(header);

    // Body (OpenDyslexic, justified, page-filled).
    const std::string &text = ctrl.getCurrentChapterText();
    if (text.empty()) {
        computeBodyGeometry();
        setTextColor(Color::BLACK);
        printAt(bodyLeft, bodyTop, "[empty chapter]");
        return;
    }

    int32_t start = pageStarts.empty() ? 0 : pageStarts[std::min(pageIdx, (int)pageStarts.size() - 1)];
    renderPage(text, (size_t)start, true);
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

// ── Typography / layout ──────────────────────────────────

void ReaderApplet::ensureBodyFont()
{
    if (!fontBodyReady) {
        // ASCII encoding: htmlToText() already reduces body text to ASCII.
        fontBody = AppletFont(OpenDyslexic14pt, AppletFont::ASCII, 0, 0);
        fontBodyReady = true;
    }
}

void ReaderApplet::computeBodyGeometry()
{
    ensureBodyFont();
    setFont(fontBody);
    bodyLeft = MARGIN_X;
    bodyWidth = (uint16_t)std::max(1, (int)width() - 2 * MARGIN_X);
    bodyTop = (int16_t)(getHeaderHeight() + MARGIN_TOP);
    bodyBottom = (int16_t)((int)height() - MARGIN_BOTTOM);
    linePitch = (int16_t)(fontBody.heightAboveCursor() + fontBody.heightBelowCursor() + LEADING);
    if (linePitch < 1)
        linePitch = 1;
    linesPerPage = (bodyBottom - bodyTop) / linePitch;
    if (linesPerPage < 1)
        linesPerPage = 1;
}

// Greedily pack words from `text` starting at `offset` into a single line <= maxW.
ReaderApplet::Line ReaderApplet::layoutLine(const std::string &text, size_t offset, uint16_t maxW)
{
    Line ln;
    size_t n = text.size();
    size_t i = offset;
    // A line/page always begins at a word; skip any leading spaces.
    while (i < n && text[i] == ' ')
        i++;
    ln.start = i;
    ln.contentEnd = i;
    ln.nextStart = i;

    const uint16_t spaceW = fontBody.widthBetweenWords();
    uint16_t curW = 0;
    int words = 0;
    size_t lastEnd = i;

    while (i < n) {
        if (text[i] == '\n') {
            i++; // consume the newline; next line starts after it
            ln.paragraphEnd = true;
            break;
        }
        size_t ws = i;
        while (i < n && text[i] != ' ' && text[i] != '\n')
            i++;
        size_t we = i;
        std::string word = text.substr(ws, we - ws);
        uint16_t ww = getTextWidth(word);
        uint16_t addW = (uint16_t)(ww + (words > 0 ? spaceW : 0));

        if (words > 0 && (uint32_t)curW + addW > maxW) {
            i = ws; // this word spills to the next line
            break;
        }
        curW = (uint16_t)(curW + addW);
        words++;
        lastEnd = we;

        if (i < n && text[i] == ' ')
            i++; // consume single separating space
        else if (i >= n) {
            ln.paragraphEnd = true; // reached end of chapter text
            break;
        }
    }

    ln.contentEnd = lastEnd;
    ln.nextStart = i;
    ln.naturalWidth = curW;
    ln.wordCount = words;
    return ln;
}

void ReaderApplet::drawLine(const std::string &text, const Line &line, int16_t top, uint16_t maxW)
{
    // Split the line's content into words.
    std::vector<std::string> words;
    size_t i = line.start;
    while (i < line.contentEnd) {
        while (i < line.contentEnd && text[i] == ' ')
            i++;
        size_t ws = i;
        while (i < line.contentEnd && text[i] != ' ')
            i++;
        if (i > ws)
            words.push_back(text.substr(ws, i - ws));
    }
    if (words.empty())
        return;

    setTextColor(Color::BLACK);
    const uint16_t spaceW = fontBody.widthBetweenWords();
    const int gaps = (int)words.size() - 1;

    // Ragged (left-aligned) for the last line of a paragraph or single-word lines.
    if (gaps <= 0 || line.paragraphEnd) {
        int16_t x = bodyLeft;
        for (const auto &w : words) {
            printAt(x, top, w, LEFT, TOP);
            x = (int16_t)(x + getTextWidth(w) + spaceW);
        }
        return;
    }

    // Full justification: distribute slack evenly across the inter-word gaps.
    uint16_t extra = (maxW > line.naturalWidth) ? (uint16_t)(maxW - line.naturalWidth) : 0;
    int base = extra / gaps;
    int rem = extra % gaps;
    int16_t x = bodyLeft;
    for (int k = 0; k < (int)words.size(); ++k) {
        printAt(x, top, words[k], LEFT, TOP);
        x = (int16_t)(x + getTextWidth(words[k]));
        if (k < gaps)
            x = (int16_t)(x + spaceW + base + (k < rem ? 1 : 0));
    }
}

size_t ReaderApplet::renderPage(const std::string &text, size_t start, bool draw)
{
    computeBodyGeometry();
    size_t s = start;
    int16_t y = bodyTop;
    for (int ln = 0; ln < linesPerPage; ++ln) {
        if (s >= text.size())
            break;
        Line line = layoutLine(text, s, bodyWidth);
        if (draw)
            drawLine(text, line, y, bodyWidth);
        if (line.nextStart <= s) { // safety: no forward progress
            s = text.size();
            break;
        }
        s = line.nextStart;
        y = (int16_t)(y + linePitch);
    }
    return s;
}

// ── Pagination bookkeeping ───────────────────────────────

void ReaderApplet::syncControllerOffset()
{
    if (pageStarts.empty())
        return;
    int idx = std::min(pageIdx, (int)pageStarts.size() - 1);
    ReaderController::instance().setPageOffset(pageStarts[idx]);
}

void ReaderApplet::ensurePagination()
{
    auto &ctrl = ReaderController::instance();
    int ch = ctrl.getCurrentChapter();
    const std::string &book = ctrl.getOpenBookPath();
    if (ch == paginatedChapter && book == paginatedBook && !pageStarts.empty())
        return;

    paginatedChapter = ch;
    paginatedBook = book;
    pageStarts.assign(1, 0);
    pageIdx = 0;

    const std::string &text = ctrl.getCurrentChapterText();
    int32_t target = ctrl.getCurrentPageOffset();
    if (target <= 0 || text.empty()) {
        syncControllerOffset();
        return;
    }

    // Paginate forward from the start until a page boundary reaches/passes `target`.
    while (true) {
        int32_t s = pageStarts.back();
        if (s >= target)
            break;
        size_t ns = renderPage(text, (size_t)s, false);
        if ((int32_t)ns <= s || ns >= text.size())
            break;
        pageStarts.push_back((int32_t)ns);
    }
    // Land on the last page whose start is <= the saved offset.
    pageIdx = 0;
    for (int k = 0; k < (int)pageStarts.size(); ++k) {
        if (pageStarts[k] <= target)
            pageIdx = k;
        else
            break;
    }
    syncControllerOffset();
}

void ReaderApplet::paginateToEnd(const std::string &text)
{
    pageStarts.assign(1, 0);
    while (true) {
        int32_t s = pageStarts.back();
        if ((size_t)s >= text.size())
            break;
        size_t ns = renderPage(text, (size_t)s, false);
        if ((int32_t)ns <= s || ns >= text.size())
            break;
        pageStarts.push_back((int32_t)ns);
    }
    pageIdx = (int)pageStarts.size() - 1;
}

void ReaderApplet::readerNextPage()
{
    auto &ctrl = ReaderController::instance();
    if (!ctrl.isBookOpen())
        return;
    ensurePagination();
    const std::string &text = ctrl.getCurrentChapterText();
    int32_t cur = pageStarts.empty() ? 0 : pageStarts[std::min(pageIdx, (int)pageStarts.size() - 1)];
    size_t ns = renderPage(text, (size_t)cur, false);

    if (ns >= text.size() || (int32_t)ns <= cur) {
        // End of chapter: advance to the next one (lands on its first page).
        if (ctrl.nextChapter()) {
            paginatedChapter = -1; // force rebuild for the new chapter
            ensurePagination();
        }
        // Otherwise we're at the last page of the last chapter - stay put.
        return;
    }
    if (pageIdx + 1 >= (int)pageStarts.size())
        pageStarts.push_back((int32_t)ns);
    pageIdx++;
    syncControllerOffset();
}

void ReaderApplet::readerPrevPage()
{
    auto &ctrl = ReaderController::instance();
    if (!ctrl.isBookOpen())
        return;
    ensurePagination();

    if (pageIdx > 0) {
        pageIdx--;
        syncControllerOffset();
        return;
    }
    // At the first page of the chapter: drop into the previous chapter's last page.
    if (ctrl.prevChapter()) {
        paginatedChapter = ctrl.getCurrentChapter();
        paginatedBook = ctrl.getOpenBookPath();
        paginateToEnd(ctrl.getCurrentChapterText());
        syncControllerOffset();
    }
    // Else: very first page of the book - stay put.
}

} // namespace reader

#endif

#include "ReaderFSM.h"
#include "graphics/Screen.h"
#include "main.h"
#include <SD.h>

namespace reader
{

ReaderFSM *ReaderFSM::getInstance()
{
    static ReaderFSM instance;
    return &instance;
}

void ReaderFSM::init()
{
    LOG_INFO("ReaderFSM init");
    currentState = ReaderState::SUSPENDED;
    inputObserver.observe(inputBroker);
}

void ReaderFSM::scanForBooks()
{
    bookFiles.clear();
    selectedBookIndex = 0;
#ifdef HAS_SDCARD
    File root = SD.open("/");
    if (!root) {
        LOG_ERROR("Reader: Failed to open /sd root");
        return;
    }
    if (!root.isDirectory()) {
        LOG_ERROR("Reader: /sd root is not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            std::string filename = file.name();
            if (filename.length() > 5 && filename.substr(filename.length() - 5) == ".epub") {
                bookFiles.push_back(filename);
                LOG_INFO("Reader: Found book %s", filename.c_str());
            }
        }
        file = root.openNextFile();
    }
#endif
    if (bookFiles.empty()) {
        LOG_WARN("Reader: No EPUB files found on SD card");
    }
}

void ReaderFSM::launch()
{
    LOG_INFO("Reader: Launching...");
    scanForBooks();
    currentState = ReaderState::BOOK_LIST;
    screen->setUseDisplay(false);
    refreshDisplay();
}

void ReaderFSM::update()
{
    // State machine logic
}

void ReaderFSM::refreshDisplay()
{
    if (!screen || !screen->getDisplayDevice())
        return;
    auto display = screen->getDisplayDevice();

    switch (currentState) {
    case ReaderState::BOOK_LIST:
        display->clear();
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setFont(ArialMT_Plain_16);
        display->drawString(0, 0, "Select Book:");
        display->setFont(ArialMT_Plain_10);
        if (bookFiles.empty()) {
            display->drawString(10, 30, "(No .epub files found)");
        } else {
            for (size_t i = 0; i < bookFiles.size() && i < 10; i++) {
                std::string entry = ((int)i == selectedBookIndex ? "> " : "  ") + bookFiles[i];
                display->drawString(0, 30 + (i * 15), entry.c_str());
            }
        }
        display->display();
        break;
    case ReaderState::READING:
        if (currentChapterText.empty()) {
            currentChapterText = parser.getChapter(currentChapter);
        }
        renderer.renderPage(display, currentChapterText, currentPageOffset);
        break;
    default:
        break;
    }
}

int ReaderFSM::handleInputEvent(const InputEvent *event)
{
    if (currentState == ReaderState::SUSPENDED)
        return 0;

    LOG_DEBUG("ReaderFSM input event: %u", event->inputEvent);

    switch (currentState) {
    case ReaderState::BOOK_LIST:
        if (event->inputEvent == INPUT_BROKER_SELECT) {
            if (!bookFiles.empty() && selectedBookIndex < (int)bookFiles.size()) {
                std::string path = "/sd/" + bookFiles[selectedBookIndex];
                if (parser.open(path)) {
                    currentState = ReaderState::READING;
                    currentChapter = 0;
                    currentPageOffset = 0;
                    currentChapterText = "";
                    LOG_INFO("Reader: Opening %s", path.c_str());
                    refreshDisplay();
                } else {
                    LOG_ERROR("Reader: Failed to open %s", path.c_str());
                }
            }
        } else if (event->inputEvent == INPUT_BROKER_DOWN) {
            if (!bookFiles.empty()) {
                selectedBookIndex = (selectedBookIndex + 1) % bookFiles.size();
                refreshDisplay();
            }
        } else if (event->inputEvent == INPUT_BROKER_UP) {
            if (!bookFiles.empty()) {
                selectedBookIndex = (selectedBookIndex + bookFiles.size() - 1) % bookFiles.size();
                refreshDisplay();
            }
        } else if (event->inputEvent == INPUT_BROKER_BACK || event->inputEvent == INPUT_BROKER_ALT_PRESS) {
            currentState = ReaderState::SUSPENDED;
            screen->setUseDisplay(true);
            screen->runNow();
            LOG_INFO("Reader: Exiting to BaseUI");
        }
        break;
    case ReaderState::READING:
        if (event->inputEvent == INPUT_BROKER_BACK) {
            currentState = ReaderState::BOOK_LIST;
            LOG_INFO("Reader: Returning to BOOK_LIST");
            refreshDisplay();
        } else if (event->inputEvent == INPUT_BROKER_DOWN || event->inputEvent == INPUT_BROKER_USER_PRESS) {
            // Next page
            currentPageOffset += 500; // Naive char offset
            if (currentPageOffset >= (int)currentChapterText.length()) {
                if (currentChapter < parser.getChapterCount() - 1) {
                    currentChapter++;
                    currentPageOffset = 0;
                    currentChapterText = "";
                    LOG_INFO("Reader: Moving to next chapter %d", currentChapter);
                } else {
                    currentPageOffset = currentChapterText.length() - 100; // Cap at end
                }
            }
            LOG_INFO("Reader: Next page, offset %d", currentPageOffset);
            refreshDisplay();
        } else if (event->inputEvent == INPUT_BROKER_UP) {
            // Previous page
            currentPageOffset -= 500;
            if (currentPageOffset < 0) {
                if (currentChapter > 0) {
                    currentChapter--;
                    currentChapterText = parser.getChapter(currentChapter);
                    currentPageOffset = std::max(0, (int)currentChapterText.length() - 500);
                    LOG_INFO("Reader: Moving to previous chapter %d", currentChapter);
                } else {
                    currentPageOffset = 0;
                }
            }
            LOG_INFO("Reader: Previous page, offset %d", currentPageOffset);
            refreshDisplay();
        }
        break;
    default:
        break;
    }
    return 0;
}

void ReaderFSM::nextTile()
{ /* logic */
}
void ReaderFSM::shortpress()
{ /* logic */
}
void ReaderFSM::longpress()
{ /* logic */
}

} // namespace reader

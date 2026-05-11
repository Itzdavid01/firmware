#include "ReaderFSM.h"
#include "gps/RTC.h"
#include "graphics/Screen.h"
#include "main.h"
#include <SD.h>
#include <algorithm>

#include <dirent.h>
#include <sys/stat.h>

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
#ifdef HAS_SDCARD
    LOG_INFO("ReaderFSM: HAS_SDCARD is defined");
    scanForBooks();
#else
    LOG_INFO("ReaderFSM: HAS_SDCARD is NOT defined");
#endif
    currentState = ReaderState::SUSPENDED;
    inputObserver.observe(inputBroker);
}

void ReaderFSM::scanForBooks()
{
    bookFiles.clear();
    selectedBookIndex = 0;
#ifdef HAS_SDCARD
    LOG_INFO("Reader: Scanning /sd for books...");

    DIR *dir = opendir("/sd");
    if (!dir) {
        LOG_ERROR("Reader: Failed to opendir /sd - error: %s", strerror(errno));
    } else {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string filename = ent->d_name;
            LOG_INFO("Reader: VFS found entry [%s], type %d", filename.c_str(), ent->d_type);

            if (ent->d_type == DT_REG || ent->d_type == DT_UNKNOWN) {
                if (filename.length() > 5) {
                    std::string ext = filename.substr(filename.length() - 5);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".epub") {
                        bookFiles.push_back(filename);
                        LOG_INFO("Reader: Added book %s", filename.c_str());
                    }
                }
            }
        }
        closedir(dir);
    }

    LOG_INFO("Reader: Finished scan. Found %u epubs", bookFiles.size());
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

void ReaderFSM::update() {}

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
                display->drawString(0, 30 + (int)(i * 15), entry.c_str());
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
                    openBookPath = path;
                    currentState = ReaderState::READING;
                    currentChapter = 0;
                    currentPageOffset = 0;
                    currentChapterText = "";
                    LOG_INFO("Reader: Opening %s", path.c_str());
                    loadProgress();
                    refreshDisplay();
                } else {
                    LOG_ERROR("Reader: Failed to open %s", path.c_str());
                }
            }
        } else if (event->inputEvent == INPUT_BROKER_DOWN) {
            if (!bookFiles.empty()) {
                selectedBookIndex = (selectedBookIndex + 1) % (int)bookFiles.size();
                refreshDisplay();
            }
        } else if (event->inputEvent == INPUT_BROKER_UP) {
            if (!bookFiles.empty()) {
                selectedBookIndex = (selectedBookIndex + (int)bookFiles.size() - 1) % (int)bookFiles.size();
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
            saveProgress();
            currentState = ReaderState::BOOK_LIST;
            LOG_INFO("Reader: Returning to BOOK_LIST");
            refreshDisplay();
        } else if (event->inputEvent == INPUT_BROKER_DOWN || event->inputEvent == INPUT_BROKER_USER_PRESS) {
            currentPageOffset += 500;
            if (currentPageOffset >= (int)currentChapterText.length()) {
                if (currentChapter < parser.getChapterCount() - 1) {
                    currentChapter++;
                    currentPageOffset = 0;
                    currentChapterText = "";
                    LOG_INFO("Reader: Moving to next chapter %d", currentChapter);
                } else {
                    currentPageOffset = (int)currentChapterText.length() - 100;
                }
            }
            LOG_INFO("Reader: Next page, offset %d", currentPageOffset);
            if (millis() - lastProgressSaveMs >= PROGRESS_SAVE_DEBOUNCE_MS) {
                saveProgress();
            }
            refreshDisplay();
        } else if (event->inputEvent == INPUT_BROKER_UP) {
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
            if (millis() - lastProgressSaveMs >= PROGRESS_SAVE_DEBOUNCE_MS) {
                saveProgress();
            }
            refreshDisplay();
        }
        break;
    default:
        break;
    }
    return 0;
}

void ReaderFSM::saveProgress()
{
    if (openBookPath.empty())
        return;
    ReadingProgress progress;
    progress.chapterIndex = currentChapter;
    progress.pageOffset = currentPageOffset;
    progress.timestamp = getTime();
    if (saveReadingProgress(openBookPath, progress)) {
        lastProgressSaveMs = millis();
        LOG_INFO("Reader: Saved progress ch=%d off=%d", currentChapter, currentPageOffset);
    } else {
        LOG_WARN("Reader: Failed to save progress");
    }
}

void ReaderFSM::loadProgress()
{
    if (openBookPath.empty())
        return;
    ReadingProgress progress;
    if (loadReadingProgress(openBookPath, progress)) {
        if (progress.chapterIndex >= 0 && progress.chapterIndex < parser.getChapterCount()) {
            currentChapter = progress.chapterIndex;
            currentPageOffset = std::max(0, progress.pageOffset);
            currentChapterText = "";
            LOG_INFO("Reader: Loaded progress ch=%d off=%d", currentChapter, currentPageOffset);
        } else {
            LOG_INFO("Reader: Saved progress out of range, starting from beginning");
        }
    } else {
        LOG_INFO("Reader: No saved progress found, starting from beginning");
    }
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

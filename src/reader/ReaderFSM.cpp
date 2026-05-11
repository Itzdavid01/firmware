#include "ReaderFSM.h"
#include "gps/RTC.h"
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

namespace reader
{

ReaderController::ReaderController() = default;

ReaderController &ReaderController::instance()
{
    static ReaderController inst;
    return inst;
}

// ── Book list ─────────────────────────────────────────

void ReaderController::scanForBooks()
{
    bookFiles.clear();
    selectedBookIndex = 0;
#ifdef HAS_SDCARD
    DIR *dir = opendir("/sd");
    if (!dir)
        return;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string filename = ent->d_name;
        if ((ent->d_type == DT_REG || ent->d_type == DT_UNKNOWN) && filename.length() > 5) {
            std::string ext = filename.substr(filename.length() - 5);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".epub") {
                bookFiles.push_back("/sd/" + filename);
            }
        }
    }
    closedir(dir);
#endif
}

const std::vector<std::string> &ReaderController::getBookFiles() const
{
    return bookFiles;
}
int ReaderController::getSelectedBookIndex() const
{
    return selectedBookIndex;
}

void ReaderController::selectNextBook()
{
    if (!bookFiles.empty())
        selectedBookIndex = (selectedBookIndex + 1) % (int)bookFiles.size();
}

void ReaderController::selectPrevBook()
{
    if (!bookFiles.empty())
        selectedBookIndex = (selectedBookIndex + (int)bookFiles.size() - 1) % (int)bookFiles.size();
}

// ── Reading ──────────────────────────────────────────

bool ReaderController::openBook(size_t index)
{
    if (index >= bookFiles.size())
        return false;
    openBookPath = bookFiles[index];
    if (!parser.open(openBookPath))
        return false;
    currentChapter = 0;
    currentPageOffset = 0;
    refreshChapterText();
    loadProgress();
    return true;
}

void ReaderController::closeBook()
{
    saveProgress();
    openBookPath.clear();
    currentChapterText.clear();
    parser.invalidateChapterCache();
}

bool ReaderController::isBookOpen() const
{
    return !openBookPath.empty();
}
const std::string &ReaderController::getOpenBookPath() const
{
    return openBookPath;
}
int ReaderController::getCurrentChapter() const
{
    return currentChapter;
}
int ReaderController::getCurrentPageOffset() const
{
    return currentPageOffset;
}
const std::string &ReaderController::getCurrentChapterText() const
{
    return currentChapterText;
}
int ReaderController::getChapterCount() const
{
    return parser.getChapterCount();
}

void ReaderController::refreshChapterText()
{
    if (!openBookPath.empty())
        currentChapterText = parser.getChapter(currentChapter);
}

// ── Navigation ────────────────────────────────────────

void ReaderController::nextPage()
{
    if (!isBookOpen())
        return;
    int textLen = (int)currentChapterText.length();
    int step = 500;

    if (currentPageOffset + step >= textLen) {
        if (currentChapter < parser.getChapterCount() - 1) {
            currentChapter++;
            currentPageOffset = 0;
            refreshChapterText();
        } else {
            currentPageOffset = std::max(0, textLen - 100);
        }
    } else {
        int target = currentPageOffset + step;
        while (target < textLen && currentChapterText[target] != ' ' && currentChapterText[target] != '\n')
            target++;
        currentPageOffset = target;
    }
}

void ReaderController::prevPage()
{
    if (!isBookOpen())
        return;
    int step = 500;

    if (currentPageOffset <= step) {
        if (currentChapter > 0) {
            currentChapter--;
            refreshChapterText();
            currentPageOffset = std::max(0, (int)currentChapterText.length() - step);
        } else {
            currentPageOffset = 0;
        }
    } else {
        int target = currentPageOffset - step;
        while (target > 0 && currentChapterText[target - 1] != ' ' && currentChapterText[target - 1] != '\n')
            target--;
        currentPageOffset = target;
    }
}

// ── Progress persistence ─────────────────────────────────

ReaderController::Progress ReaderController::getProgress() const
{
    return {currentChapter, currentPageOffset, (uint32_t)getTime()};
}

void ReaderController::setProgress(const Progress &p)
{
    currentChapter = p.chapterIndex;
    currentPageOffset = p.pageOffset;
    if (openBookPath.empty())
        return;

    uint32_t now = millis();
    if (now - lastProgressSaveMs < PROGRESS_SAVE_DEBOUNCE_MS && p.chapterIndex == currentChapter)
        return;
    lastProgressSaveMs = now;

    saveReadingProgress(openBookPath, {p.chapterIndex, p.pageOffset, p.timestamp});
}

void ReaderController::loadProgress()
{
    if (openBookPath.empty())
        return;
    ReadingProgress p;
    if (loadReadingProgress(openBookPath, p)) {
        if (p.chapterIndex >= 0 && p.chapterIndex < parser.getChapterCount()) {
            currentChapter = p.chapterIndex;
            currentPageOffset = std::max(0, p.pageOffset);
            refreshChapterText();
        }
    }
}

void ReaderController::saveProgress()
{
    if (openBookPath.empty())
        return;
    auto p = getProgress();
    saveReadingProgress(openBookPath, {p.chapterIndex, p.pageOffset, p.timestamp});
}

} // namespace reader
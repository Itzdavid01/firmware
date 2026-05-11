#pragma once
#include "ChapterCache.h"
#include "EpubParser.h"
#include "ProgressStore.h"
#include <string>
#include <vector>

// Forward declare Applet (used in ReaderApplet, not here)
class ReaderApplet;

namespace reader
{

// Pure state machine — no display, no input, no Applet dependency.
// Singleton accessed via ReaderController::instance().
class ReaderController
{
  public:
    static ReaderController &instance();

    // ── Book list ─────────────────────────────────────────
    void scanForBooks();
    const std::vector<std::string> &getBookFiles() const;
    int getSelectedBookIndex() const;
    void selectNextBook();
    void selectPrevBook();

    // ── Reading ──────────────────────────────────────────
    bool openBook(size_t index); // returns false on error
    void closeBook();
    bool isBookOpen() const;
    const std::string &getOpenBookPath() const;
    int getCurrentChapter() const;
    int getCurrentPageOffset() const;
    const std::string &getCurrentChapterText() const;
    int getChapterCount() const;

    // ── Navigation ────────────────────────────────────────
    void nextPage(); // advance ~500 chars or next chapter
    void prevPage(); // retreat ~500 chars or prev chapter

    // ── Progress persistence ─────────────────────────────
    struct Progress {
        int chapterIndex = 0;
        int pageOffset = 0;
        uint32_t timestamp = 0;
    };
    Progress getProgress() const;
    void setProgress(const Progress &p);

  private:
    friend class ReaderApplet;
    ReaderController();
    ~ReaderController() = default;
    ReaderController(const ReaderController &) = delete;
    ReaderController &operator=(const ReaderController &) = delete;

    EpubParser parser;
    int currentChapter = 0;
    int currentPageOffset = 0;
    std::string currentChapterText;
    std::string openBookPath;
    std::vector<std::string> bookFiles;
    int selectedBookIndex = 0;
    uint32_t lastProgressSaveMs = 0;
    static constexpr uint32_t PROGRESS_SAVE_DEBOUNCE_MS = 2000;

    void refreshChapterText();
    void loadProgress();
    void saveProgress();
};

// Deprecated alias — existing code that references ReaderFSM still works.
class ReaderFSM
{
  public:
    static ReaderFSM *getInstance() { return nullptr; } // Returns nullptr — migration aid
    void init() { ReaderController::instance().scanForBooks(); }
    void launch() {}
    void update() {}
    void nextTile() {}
    void shortpress() {}
    void longpress() {}
};

} // namespace reader
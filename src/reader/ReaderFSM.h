#pragma once

#include "Observer.h"
#include "configuration.h"
#include "input/InputBroker.h"

#include "DocumentRenderer.h"
#include "EpubParser.h"
#include "ProgressStore.h"

namespace reader
{

enum class ReaderState { SUSPENDED, BOOK_LIST, READING, SETTINGS };

class ReaderFSM
{
  public:
    static ReaderFSM *getInstance();
    void init();
    void launch();
    void update();

    void nextTile();
    void shortpress();
    void longpress();

  private:
    ReaderFSM() = default;
    ReaderFSM(const ReaderFSM &) = delete;
    ReaderFSM &operator=(const ReaderFSM &) = delete;
    ReaderState currentState = ReaderState::SUSPENDED;

    EpubParser parser;
    DocumentRenderer renderer;

    int currentChapter = 0;
    int currentPageOffset = 0;
    std::string currentChapterText;

    CallbackObserver<ReaderFSM, const InputEvent *> inputObserver =
        CallbackObserver<ReaderFSM, const InputEvent *>(this, &ReaderFSM::handleInputEvent);

    void scanForBooks();
    int handleInputEvent(const InputEvent *event);
    void refreshDisplay();
    void saveProgress();
    void loadProgress();

    std::vector<std::string> bookFiles;
    int selectedBookIndex = 0;
    std::string openBookPath;
    uint32_t lastProgressSaveMs = 0;
    static constexpr uint32_t PROGRESS_SAVE_DEBOUNCE_MS = 2000;
};

} // namespace reader

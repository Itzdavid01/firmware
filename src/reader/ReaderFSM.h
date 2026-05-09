#pragma once

#include "Observer.h"
#include "configuration.h"
#include "input/InputBroker.h"

#include "DocumentRenderer.h"
#include "EpubParser.h"

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
    ReaderState currentState = ReaderState::BOOK_LIST;

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

    std::vector<std::string> bookFiles;
    int selectedBookIndex = 0;
};

} // namespace reader

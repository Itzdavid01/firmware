#ifdef MESHTASTIC_INCLUDE_INKHUD

#pragma once
#include <cstdint>
#include <string>

namespace reader
{

class DocumentRenderer
{
  public:
    DocumentRenderer() = default;
    std::string prepareChapterText(const std::string &rawChapter, int pageOffset) const;
    int calculatePages(const std::string & /*text*/) const { return 1; }

  private:
    int fontSize = 16;
};

} // namespace reader

#endif
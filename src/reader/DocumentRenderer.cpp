#ifdef MESHTASTIC_INCLUDE_INKHUD
#include "DocumentRenderer.h"
#include <string>

namespace reader
{

std::string DocumentRenderer::prepareChapterText(const std::string &rawChapter, int pageOffset) const
{
    if (rawChapter.empty())
        return {};

    std::string clean;
    clean.reserve(rawChapter.size());
    bool inTag = false;
    for (char c : rawChapter) {
        if (c == '<') {
            inTag = true;
            continue;
        }
        if (c == '>') {
            inTag = false;
            continue;
        }
        if (!inTag)
            clean.push_back(c);
    }

    if (pageOffset <= 0 || pageOffset >= (int)clean.size())
        return clean;

    int lineStart = pageOffset;
    while (lineStart > 0 && clean[lineStart - 1] != ' ' && clean[lineStart - 1] != '\n')
        lineStart--;
    return clean.substr(lineStart);
}

} // namespace reader
#endif
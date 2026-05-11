#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "DocumentRenderer.h"
#include "graphics/niche/InkHUD/Applet.h"

namespace reader
{

void DocumentRenderer::renderPage(NicheGraphics::InkHUD::Applet *applet, const std::string &text, int pageOffset)
{
    if (!applet || text.empty())
        return;

    applet->resetDrawingSpace();
    applet->setTextColor(NicheGraphics::InkHUD::Color::BLACK);

    // Strip HTML tags in a single pass
    std::string clean;
    clean.reserve(text.size());
    bool inTag = false;
    for (char c : text) {
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

    // Align to word boundary from pageOffset
    std::string displayText = clean;
    if (pageOffset > 0 && pageOffset < (int)clean.size()) {
        int lineStart = pageOffset;
        while (lineStart > 0 && clean[lineStart - 1] != ' ' && clean[lineStart - 1] != '\n')
            lineStart--;
        displayText = clean.substr(lineStart);
    }

    // Draw wrapped text starting below the header
    int16_t top = applet->getHeaderHeight() + 6;
    uint16_t maxW = applet->X(1.0f) - 8;
    applet->printWrapped(4, top, maxW, displayText);
}

} // namespace reader

#endif
#include "DocumentRenderer.h"
#include "main.h"

namespace reader
{

void DocumentRenderer::renderPage(OLEDDisplay *display, const std::string &text, int pageOffset)
{
    LOG_INFO("DocumentRenderer: Rendering page at offset %d", pageOffset);

    // Simple HTML tag stripper - Optimized for memory
    std::string cleanText;
    cleanText.reserve(text.length()); // Prevent O(N^2) reallocations
    bool inTag = false;
    for (char c : text) {
        if (c == '<')
            inTag = true;
        else if (c == '>')
            inTag = false;
        else if (!inTag)
            cleanText += c;
    }

    display->clear();
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_16); // Larger font for readability

    // Draw string with automatic wrapping
    // Skip 'pageOffset' characters using pointer arithmetic to avoid massive string copies
    if (pageOffset < (int)cleanText.length()) {
        display->drawStringMaxWidth(0, 0, display->getWidth(), cleanText.c_str() + pageOffset);
    }
    display->display();
}

int DocumentRenderer::calculatePages(OLEDDisplay *display, const std::string &text)
{
    // Stub: for now just assume each chapter is roughly one page or scrollable
    return 1;
}

} // namespace reader

#pragma once

#include <OLEDDisplay.h>
#include <string>
#include <vector>

namespace reader
{

class DocumentRenderer
{
  public:
    DocumentRenderer() = default;
    void renderPage(OLEDDisplay *display, const std::string &text, int pageOffset);
    int calculatePages(OLEDDisplay *display, const std::string &text);

  private:
    int fontSize = 10;
};

} // namespace reader

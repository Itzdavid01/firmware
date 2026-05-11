#ifdef MESHTASTIC_INCLUDE_INKHUD

#pragma once
#include <cstdint>
#include <string>

namespace NicheGraphics::InkHUD
{
class Applet;
}

namespace reader
{

class DocumentRenderer
{
  public:
    DocumentRenderer() = default;

    void renderPage(NicheGraphics::InkHUD::Applet *applet, const std::string &text, int pageOffset);

    int calculatePages(NicheGraphics::InkHUD::Applet * /*display*/, const std::string & /*text*/) { return 1; }

  private:
    int fontSize = 16;
};

} // namespace reader

#endif
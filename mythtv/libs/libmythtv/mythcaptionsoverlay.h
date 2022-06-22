#ifndef MYTHCAPTIONSOVERLAY_H
#define MYTHCAPTIONSOVERLAY_H

// MythTV
#include "mythmediaoverlay.h"

static constexpr const char* OSD_WIN_TELETEXT  { "aa_OSD_TELETEXT"    };
static constexpr const char* OSD_WIN_SUBTITLE  { "aa_OSD_SUBTITLES"   };
static constexpr const char* OSD_WIN_INTERACT  { "bb_OSD_INTERACTIVE" };
static constexpr const char* OSD_WIN_BDOVERLAY { "bb_OSD_BDOVERLAY"   };

class TeletextScreen;
class SubtitleScreen;
class MythBDOverlay;
struct AVSubtitle;

class MythCaptionsOverlay : public MythMediaOverlay
{
    Q_OBJECT

  public:
    MythCaptionsOverlay(MythMainWindow* MainWindow, TV* Tv, MythPlayerUI* Player, MythPainter* Painter);
   ~MythCaptionsOverlay() override;

    void Draw(QRect Rect);

    MythScreenType* GetWindow(const QString& Window) override;

    TeletextScreen* InitTeletext();
    void EnableTeletext(bool Enable, int Page);
    bool TeletextAction(const QString& Action, bool& Exit);
    void TeletextReset();
    void TeletextClear();

    SubtitleScreen* InitSubtitles();
    void EnableSubtitles(int Type, bool ForcedOnly = false);
    void DisableForcedSubtitles();
    void ClearSubtitles();
    void DisplayDVDButton(AVSubtitle* DVDButton, QRect& Pos);
    void DisplayBDOverlay(MythBDOverlay* Overlay);

  protected:
    void TearDown() override;
};

#endif

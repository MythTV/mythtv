#ifndef MYTHCAPTIONSOVERLAY_H
#define MYTHCAPTIONSOVERLAY_H

// MythTV
#include "mythmediaoverlay.h"

#define OSD_WIN_TELETEXT  "aa_OSD_TELETEXT"
#define OSD_WIN_SUBTITLE  "aa_OSD_SUBTITLES"
#define OSD_WIN_INTERACT  "bb_OSD_INTERACTIVE"
#define OSD_WIN_BDOVERLAY "bb_OSD_BDOVERLAY"

class TeletextScreen;
class SubtitleScreen;
class MythBDOverlay;
struct AVSubtitle;

class MythCaptionsOverlay : public MythMediaOverlay
{
  public:
    MythCaptionsOverlay(MythMainWindow* MainWindow, TV* Tv, MythPlayerUI* Player, MythPainter* Painter);
   ~MythCaptionsOverlay() override;

    MythScreenType* GetWindow(const QString& Window) override;

    TeletextScreen* InitTeletext();
    void EnableTeletext(bool Enable, int Page);
    bool TeletextAction(const QString& Action);
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

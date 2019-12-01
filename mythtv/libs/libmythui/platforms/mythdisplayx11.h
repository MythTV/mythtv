#ifndef MYTHDISPLAYX11_H
#define MYTHDISPLAYX11_H

// MythTV
#include "mythdisplay.h"

class MythDisplayX11 : public MythDisplay
{
  public:
    MythDisplayX11();
   ~MythDisplayX11() override;
    static bool IsAvailable(void);
    DisplayInfo GetDisplayInfo(int VideoRate = 0) override;

#ifdef USING_XRANDR
    bool UsingVideoModes(void) override;
    const std::vector<DisplayResScreen>& GetVideoModes(void) override;
    bool SwitchToVideoMode(int Width, int Height, double DesiredRate) override;
#endif

  private:
    void DebugModes(const QString& Message) const;

    mutable std::vector<DisplayResScreen> m_videoModesUnsorted { };
};

#endif // MYTHDISPLAYX11_H

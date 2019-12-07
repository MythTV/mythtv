#ifndef MYTHDISPLAYX11_H
#define MYTHDISPLAYX11_H

// Qt
#include <QMap>

// MythTV
#include "mythdisplay.h"

class MythDisplayX11 : public MythDisplay
{
  public:
    MythDisplayX11();
   ~MythDisplayX11() override = default;
    static bool IsAvailable(void);
    DisplayInfo GetDisplayInfo(int VideoRate = 0) override;

#ifdef USING_XRANDR
    bool UsingVideoModes(void) override;
    const std::vector<MythDisplayMode>& GetVideoModes(void) override;
    bool SwitchToVideoMode(int Width, int Height, double DesiredRate) override;
#endif

  private:
    QMap<uint64_t, unsigned long> m_modeMap { };
    unsigned long m_crtc { 0 };
};

#endif // MYTHDISPLAYX11_H

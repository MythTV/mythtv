#ifndef MYTHDISPLAYX11_H
#define MYTHDISPLAYX11_H

// Qt
#include <QMap>

// MythTV
#include "mythdisplay.h"
#include "mythxdisplay.h"

#ifdef USING_XRANDR
#include <X11/extensions/Xrandr.h> // always last
#endif

class MythDisplayX11 : public MythDisplay
{
  public:
    MythDisplayX11();
   ~MythDisplayX11() override = default;
    static bool IsAvailable(void);
    void UpdateCurrentMode(void) override;

#ifdef USING_XRANDR
    bool VideoModesAvailable(void) override { return true; }
    bool UsingVideoModes(void) override;
    const std::vector<MythDisplayMode>& GetVideoModes(void) override;
    bool SwitchToVideoMode(QSize Size, double DesiredRate) override;

  private:
    static XRROutputInfo* GetOutput(XRRScreenResources* Resources, MythXDisplay* mDisplay,
                             QScreen* qScreen, RROutput* Output = nullptr);
#endif

  private:
    void GetEDID(MythXDisplay* mDisplay);

    QMap<uint64_t, unsigned long> m_modeMap { };
    unsigned long m_crtc { 0 };
};

#endif // MYTHDISPLAYX11_H

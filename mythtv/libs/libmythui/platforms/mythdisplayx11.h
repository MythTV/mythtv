#ifndef MYTHDISPLAYX11_H
#define MYTHDISPLAYX11_H

// Qt
#include <QMap>

// MythTV
#include "mythdisplay.h"
#include "mythxdisplay.h"

// X11 - always last
#define pointer Xpointer // Prevent conflicts with Qt6.
#include <X11/extensions/Xrandr.h>
#undef pointer

class MythDisplayX11 : public MythDisplay
{
  public:
    MythDisplayX11();
   ~MythDisplayX11() override = default;

    static bool IsAvailable ();
    void UpdateCurrentMode  () override;
    bool VideoModesAvailable() override { return true; }
    bool UsingVideoModes    () override;
    bool SwitchToVideoMode  (QSize Size, double DesiredRate) override;
    const MythDisplayModes& GetVideoModes() override;

  private:
    static XRROutputInfo* GetOutput(XRRScreenResources* Resources, MythXDisplay* mDisplay,
                                    QScreen* qScreen, RROutput* Output = nullptr);
    void GetEDID(MythXDisplay* mDisplay);

    QMap<uint64_t, unsigned long> m_modeMap;
    unsigned long m_crtc { 0 };
};

#endif

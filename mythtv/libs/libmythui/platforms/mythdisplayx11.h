#ifndef MYTHDISPLAYX11_H
#define MYTHDISPLAYX11_H

// Qt
#include <QMap>

// MythTV
#include "mythdisplay.h"
class MythXDisplay;

class MythDisplayX11 : public MythDisplay
{
  public:
    MythDisplayX11();
   ~MythDisplayX11() override = default;

    static bool IsAvailable ();
    bool VideoModesAvailable() override { return true; }
    bool UsingVideoModes    () override;
    const MythDisplayModes& GetVideoModes() override;

  protected:
    void UpdateCurrentMode  () override;
    bool SwitchToVideoMode  (QSize Size, double DesiredRate) override;

  private:
    void GetEDID(MythXDisplay* mDisplay);

    QMap<uint64_t, unsigned long> m_modeMap;
    unsigned long m_crtc { 0 };
};

#endif

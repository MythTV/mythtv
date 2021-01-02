#ifndef MYTHDISPLAYOSX_H
#define MYTHDISPLAYOSX_H

// MythTV
#include "mythdisplay.h"

// CoreGraphics
#include <CoreGraphics/CGDirectDisplay.h>

class MythDisplayOSX : public MythDisplay
{
  public:
    MythDisplayOSX();
   ~MythDisplayOSX() override;

    void UpdateCurrentMode(void) override;

    bool VideoModesAvailable(void) override { return true; }
    bool UsingVideoModes(void) override;
    const MythDisplayModes& GetVideoModes(void) override;
    bool SwitchToVideoMode(QSize Size, double DesiredRate) override;

  private:
    void ClearModes(void);
    QMap<uint64_t, CGDisplayModeRef> m_modeMap { };
};

#endif // MYTHDISPLAYOSX_H

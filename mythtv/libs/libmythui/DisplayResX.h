#ifndef _DISPLAYRESX_H_
#define _DISPLAYRESX_H_

#include <QMap>

#include "DisplayRes.h"

class DisplayResX : public DisplayRes
{
  public:
    DisplayResX(void);
    ~DisplayResX(void);

    const std::vector<DisplayResScreen>& GetVideoModes(void);

  protected:
    bool GetDisplayInfo(int &w_pix, int &h_pix, int &w_mm,
                        int &h_mm, double &rate, double &par) const;
    bool SwitchToVideoMode(int width, int height, double framerate);

  private:
    mutable std::vector<DisplayResScreen> m_videoModes;
    QMap<uint64_t, unsigned long> m_modeMap { };
    unsigned long m_crtc { 0 };
};

#endif // _DISPLAYRESX_H_

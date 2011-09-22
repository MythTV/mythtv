#ifndef _DISPLAYRESOSX_H_
#define _DISPLAYRESOSX_H_

#include "DisplayRes.h"

class DisplayResOSX : public DisplayRes
{
  public:
    DisplayResOSX(void);
    ~DisplayResOSX(void);

    const std::vector<DisplayResScreen>& GetVideoModes() const;

  protected:
    bool GetDisplayInfo(int &w_pix, int &h_pix, int &w_mm,
                        int &h_mm, double &rate, double &par) const;
    bool SwitchToVideoMode(int width, int height, double framerate);

  private:
    mutable std::vector<DisplayResScreen> m_videoModes;
};

#endif // _DISPLAYRESOSX_H_

#ifndef _DISPLAYRESOSX_H_
#define _DISPLAYRESOSX_H_

#include "DisplayRes.h"

class DisplayResOSX : public DisplayRes
{
  public:
    DisplayResOSX(void);
    ~DisplayResOSX(void) = default;

    const std::vector<DisplayResScreen>& GetVideoModes() const override; // DisplayRes

  protected:
    bool GetDisplayInfo(int &w_pix, int &h_pix, int &w_mm,
                        int &h_mm, double &rate, double &par) const override; // DisplayRes
    bool SwitchToVideoMode(int width, int height, double framerate) override; // DisplayRes

  private:
    mutable std::vector<DisplayResScreen> m_videoModes;
};

#endif // _DISPLAYRESOSX_H_

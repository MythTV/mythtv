#ifndef _DISPLAYRESX_H_
#define _DISPLAYRESX_H_

#include "DisplayRes.h"

class DisplayResX : public DisplayRes {
  public:
    DisplayResX(void);
    ~DisplayResX(void);

    const std::vector<DisplayResScreen>& GetVideoModes(void) const;

  protected:
    bool GetDisplayInfo(int &w_pix, int &h_pix, int &w_mm,
                        int &h_mm, double &rate, double &par) const;
    bool SwitchToVideoMode(int width, int height, double framerate);

  private:
    mutable std::vector<DisplayResScreen> m_video_modes;
    mutable std::vector<DisplayResScreen> m_video_modes_unsorted;
};

#endif // _DISPLAYRESX_H_

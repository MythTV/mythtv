#ifndef _DISPLAYRESX_H_
#define _DISPLAYRESX_H_

#include "DisplayRes.h"

class DisplayResX : public DisplayRes {
  public:
    DisplayResX(void);
    ~DisplayResX(void);

    const vector<DisplayResScreen>& GetVideoModes(void) const;

  protected:
    bool GetDisplaySize(int &width_mm, int &height_mm) const;
    bool SwitchToVideoMode(int width, int height, short framerate);

  private:
    mutable vector<DisplayResScreen> m_video_modes;
    mutable vector<DisplayResScreen> m_video_modes_unsorted;
};

#endif // _DISPLAYRESX_H_

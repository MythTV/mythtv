#ifndef _DISPLAYRESOSX_H_
#define _DISPLAYRESOSX_H_

#include "DisplayRes.h"

class DisplayResOSX : public DisplayRes {
  public:
    DisplayResOSX(void);
    ~DisplayResOSX(void);

    const vector<DisplayResScreen>& GetVideoModes() const;

  protected:
    bool GetDisplaySize(int &width_mm, int &height_mm) const;
    bool SwitchToVideoMode(int width, int height, short framerate);
    
  private:
    mutable vector<DisplayResScreen> m_video_modes;
};

#endif // _DISPLAYRESOSX_H_

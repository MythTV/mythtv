#ifndef _DISPLAYRESX_H_
#define _DISPLAYRESX_H_

#include "DisplayRes.h"

class DisplayResX : public DisplayRes {
  protected:
    bool get_display_size(int & width_mm, int & height_mm);
    bool switch_res(int width, int height);
  
  public:
    DisplayResX(void);
    ~DisplayResX(void);
};

#endif // _DISPLAYRESX_H_

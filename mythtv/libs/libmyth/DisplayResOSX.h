#ifndef _DISPLAYRESOSX_H_
#define _DISPLAYRESOSX_H_

#include "DisplayRes.h"

class DisplayResOSX : public DisplayRes {
  protected:
    bool get_display_size(int & width_mm, int & height_mm);
    bool switch_res(int width, int height);
    
    
  public:
    DisplayResOSX(void);
    ~DisplayResOSX(void);
};

#endif // _DISPLAYRESOSX_H_

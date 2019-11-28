#ifndef MYTHDISPLAYX11_H
#define MYTHDISPLAYX11_H

// MythTV
#include "mythdisplay.h"

class MythDisplayX11 : public MythDisplay
{
  public:
    MythDisplayX11();
   ~MythDisplayX11() override;
    static bool IsAvailable(void);
    DisplayInfo GetDisplayInfo(int VideoRate = 0) override;
};

#endif // MYTHDISPLAYX11_H

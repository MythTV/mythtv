#ifndef MYTHDISPLAYANDROID_H
#define MYTHDISPLAYANDROID_H

// MythTV
#include "mythdisplay.h"

class MythDisplayAndroid : public MythDisplay
{
  public:
    MythDisplayAndroid();
   ~MythDisplayAndroid() override;

    DisplayInfo GetDisplayInfo(int VideoRate = 0) override;
};

#endif // MYTHDISPLAYANDROID_H

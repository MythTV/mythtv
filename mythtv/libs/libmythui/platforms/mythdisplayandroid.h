#ifndef MYTHDISPLAYANDROID_H
#define MYTHDISPLAYANDROID_H

// MythTV
#include "mythdisplay.h"

class MythDisplayAndroid : public MythDisplay
{
  public:
    MythDisplayAndroid();
   ~MythDisplayAndroid() override = default;

  protected:
    void UpdateCurrentMode(void) override;
};

#endif // MYTHDISPLAYANDROID_H

#ifndef MYTHDISPLAYWINDOWS_H
#define MYTHDISPLAYWINDOWS_H

// MythTV
#include "mythdisplay.h"

class MythDisplayWindows : public MythDisplay
{
  public:
    MythDisplayWindows();
   ~MythDisplayWindows() override;

    void UpdateCurrentMode(void) override;
};

#endif // MYTHDISPLAYWINDOWS_H

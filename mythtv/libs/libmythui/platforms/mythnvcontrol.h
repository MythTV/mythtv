#ifndef UTIL_NVIDIA_H_
#define UTIL_NVIDIA_H_

// Qt
#include <QString>

#ifdef USING_XRANDR
// MythTV
#include "DisplayResScreen.h"
class MythXDisplay;

// Std
#include <vector>
using namespace std;
#endif

class MythNVControl
{
  public:
    static int  CheckNVOpenGLSyncToVBlank(void);

#ifdef USING_XRANDR
    static bool GetNvidiaRates(MythXDisplay *MythDisplay, vector<DisplayResScreen> &VideoModes);

  protected:
    static int  FindMetamode(const QStringList &Metamodes, const QString &Modename);
#endif
};
#endif

// MythTV
#include "libmythbase/mythlogging.h"
#include "platforms/mythscreensaverdrm.h"

#define LOC QString("ScreenSaverDRM: ")

MythScreenSaverDRM* MythScreenSaverDRM::Create(QObject *Parent, MythDisplay* mDisplay)
{
    auto * result = new MythScreenSaverDRM(Parent, mDisplay);
    if (result->m_valid)
        return result;
    result->setParent(nullptr);
    delete result;
    return nullptr;
}

/*! \class MythScreenSaverDRM
 *
 * This is the screensaver 'of last resort' on most linux systems. It is only
 * used when no others are available (no X, no Wayland etc).
 *
 * \note This class has been temporarily disabled whilst the DRM code is refactored
 * for atomic operations.
*/
MythScreenSaverDRM::MythScreenSaverDRM(QObject* Parent, MythDisplay *mDisplay)
  : MythScreenSaver(Parent),
    m_display(dynamic_cast<MythDisplayDRM*>(mDisplay))
{
    m_valid = m_display != nullptr;
}

void MythScreenSaverDRM::Disable()
{
}

void MythScreenSaverDRM::Restore()
{
    Disable();
}

void MythScreenSaverDRM::Reset()
{
}

bool MythScreenSaverDRM::Asleep()
{
    return false;
}

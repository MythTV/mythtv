/*----------------------------------------------------------------------------
** jsmenuevent.cpp
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**     although this is largely a derivative of lircevent.cpp
**--------------------------------------------------------------------------*/

// Own header
#include "devices/jsmenuevent.h"

const QEvent::Type JoystickKeycodeEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

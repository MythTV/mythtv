#include "mythevent.h"

QEvent::Type MythEvent::MythEventMessage =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kExitToMainMenuEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kMythPostShowEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kEnableDrawingEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kDisableDrawingEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kPushDisableDrawingEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kPopDisableDrawingEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kLockInputDevicesEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kUnlockInputDevicesEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type MythEvent::kUpdateBrowseInfoEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type ExternalKeycodeEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

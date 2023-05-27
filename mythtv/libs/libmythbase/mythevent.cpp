#include "mythevent.h"
#include "mythlogging.h"

const QEvent::Type MythEvent::kMythEventMessage =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kMythUserMessage =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kUpdateTvProgressEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kExitToMainMenuEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kMythPostShowEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kPushDisableDrawingEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kPopDisableDrawingEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kLockInputDevicesEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kUnlockInputDevicesEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kUpdateBrowseInfoEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kDisableUDPListenerEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type MythEvent::kEnableUDPListenerEventType =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type ExternalKeycodeEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

// Force this class to have a vtable so that dynamic_cast works.
// NOLINTNEXTLINE(modernize-use-equals-default)
MythEvent::~MythEvent()
{
}

void MythEvent::log(const QString& prefix)
{
    LOG(VB_GENERAL, LOG_INFO, QString("%1: %2").arg(prefix, m_message));
    if (m_extradata.isEmpty())
        return;
    if ((m_extradata.count() == 1) && (m_extradata[0] == "empty"))
        return;
    QStringList tmp = m_extradata;
    if ((m_message.startsWith("GENERATED_PIXMAP")) && (tmp[0] == "OK"))
        tmp[6]="Encoded pixmap";
    LOG(VB_GENERAL, LOG_INFO, QString("Extra data: %1").arg(tmp.join('|')));
}

#ifndef MYTHPOWEROSX_H
#define MYTHPOWEROSX_H

// Qt
#include <QObject>

// MythTV
#include "mythpower.h"

// OS X
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

class MythPowerOSX final : public MythPower
{
    Q_OBJECT

    friend class MythPower;

  public:

  protected slots:
    void Refresh (void) override;

  protected:
    MythPowerOSX();
   ~MythPowerOSX() override;
    void Init(void) override;
    bool DoFeature (bool Delayed = false) override;

    static void PowerCallBack       (void *Reference, io_service_t Service,
                                     natural_t Type, void *Data);
    static void PowerSourceCallBack (void *Reference);

    CFRunLoopSourceRef    m_powerRef        { nullptr        };
    io_connect_t          m_rootPowerDomain { 0              };
    io_object_t           m_powerNotifier   { MACH_PORT_NULL };
    IONotificationPortRef m_powerNotifyPort { nullptr        };
};

#endif // MYTHPOWEROSX_H

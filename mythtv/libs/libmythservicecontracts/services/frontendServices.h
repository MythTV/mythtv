#ifndef FRONTENDSERVICES_H
#define FRONTENDSERVICES_H

#include "service.h"
#include "datacontracts/frontendStatus.h"

class SERVICE_PUBLIC FrontendServices : public Service
{
    Q_OBJECT
    Q_CLASSINFO("version", "1.0");

  public:
    FrontendServices(QObject *parent = 0) : Service(parent)
    {
        DTC::FrontendStatus::InitializeCustomTypes();
    }

  public slots:
    virtual DTC::FrontendStatus* GetStatus(void) = 0;
};

#endif // FRONTENDSERVICES_H

#ifndef FRONTENDSERVICEHOST_H
#define FRONTENDSERVICEHOST_H

#include "servicehost.h"
#include "services/frontend.h"

class FrontendServiceHost : public ServiceHost
{
  public:
    FrontendServiceHost(const QString &sSharePath)
      : ServiceHost(Frontend::staticMetaObject, "Frontend", "/Frontend", sSharePath)
    {
    }

    virtual ~FrontendServiceHost()
    {
    }
};

#endif // FRONTENDSERVICEHOST_H

#ifndef FRONTENDSERVICEHOST_H
#define FRONTENDSERVICEHOST_H

#include "servicehost.h"
#include "services/frontend.h"

class FrontendServiceHost : public ServiceHost
{
  public:
    explicit FrontendServiceHost(const QString &sSharePath)
      : ServiceHost(Frontend::staticMetaObject, "Frontend", "/Frontend", sSharePath)
    {
    }

    ~FrontendServiceHost() override = default;
};

#endif // FRONTENDSERVICEHOST_H

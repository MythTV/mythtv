// MythTV
#include "mythlogging.h"
#include "http/mythhttpservices.h"
#include "http/mythhttpmetaservice.h"

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service, (HTTP_SERVICES_DIR, MythHTTPServices::staticMetaObject))

MythHTTPServices::MythHTTPServices()
  : MythHTTPService(s_service)
{
}

void MythHTTPServices::UpdateServices(const HTTPServices& Services)
{
    m_serviceList.clear();
    for (const auto & service : Services)
        m_serviceList.append(service.first);
    emit ServiceListChanged(m_serviceList);
}

QStringList MythHTTPServices::GetServiceList()
{
    return m_serviceList;
}

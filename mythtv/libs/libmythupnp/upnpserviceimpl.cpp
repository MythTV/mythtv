#include "upnpserviceimpl.h"
#include "upnpdevice.h"

/// Creates a UPnpService and adds it to the UPnpDevice's list of services.
void UPnpServiceImpl::RegisterService(UPnpDevice *pDevice)
{
    if (pDevice != NULL)
    {
        UPnpService *pService = new UPnpService();
            
        pService->m_sServiceType = GetServiceType();
        pService->m_sServiceId   = GetServiceId();
        pService->m_sSCPDURL     = GetServiceDescURL();
        pService->m_sControlURL  = GetServiceControlURL();
        pService->m_sEventSubURL = GetServiceEventURL();

        pDevice->m_listServices.push_back(pService);
    }
}

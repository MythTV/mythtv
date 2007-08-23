//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpImpl.h
//                                                                            
// Purpose - 
//                                                                            
// Created By  : David Blain                    Created On : Jan 15, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////


#ifndef __UPNPIMPL_H__
#define __UPNPIMPL_H__

#include "upnpdevice.h"

class UPnpServiceImpl
{
    protected:

        virtual QString     GetServiceType      () = 0;
        virtual QString     GetServiceId        () = 0;
        virtual QString     GetServiceControlURL() = 0;
        virtual QString     GetServiceDescURL   () = 0;
        virtual QString     GetServiceEventURL  () { return ""; }

    public:

        UPnpServiceImpl() {}

        void RegisterService( UPnpDevice  *pDevice )
        {
            if (pDevice != NULL)
            {
                UPnpService *pService = new UPnpService();
            
                pService->m_sServiceType = GetServiceType();
                pService->m_sServiceId   = GetServiceId();
                pService->m_sSCPDURL     = GetServiceDescURL();
                pService->m_sControlURL  = GetServiceControlURL();
                pService->m_sEventSubURL = GetServiceEventURL();

                pDevice->m_listServices.append( pService );
            }
        }
};

#endif

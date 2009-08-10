//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpImpl.h
// Created     : Jan 15, 2007
//
// Purpose     : UPnp Device Description parser/generator
//                                                                            
// Copyright (c) 2007 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
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
        virtual ~UPnpServiceImpl() {}

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

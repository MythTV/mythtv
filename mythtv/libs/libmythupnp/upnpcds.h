//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCDS.h
// Created     : Oct. 24, 2005
//
// Purpose     : 
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
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

#ifndef UPnpCDS_H_
#define UPnpCDS_H_

#include <QList>
#include <QMap>
#include <QObject>

#include "upnp.h"
#include "upnpcdsobjects.h"
#include "eventing.h"
#include "mythdbcon.h"

class UPnpCDS;

typedef enum 
{
    CDSM_Unknown                = 0,
    CDSM_GetServiceDescription  = 1,
    CDSM_Browse                 = 2,
    CDSM_Search                 = 3,
    CDSM_GetSearchCapabilities  = 4,
    CDSM_GetSortCapabilities    = 5,
    CDSM_GetSystemUpdateID      = 6

} UPnpCDSMethod;

typedef enum
{
    CDS_BrowseUnknown         = 0,
    CDS_BrowseMetadata        = 1,
    CDS_BrowseDirectChildren  = 2

} UPnpCDSBrowseFlag;

typedef enum
{
    CDS_ClientDefault         = 0,      // (no special attention required)
    CDS_ClientWMP             = 1,      // Windows Media Player
    CDS_ClientXBMC            = 2,      // XBMC
    CDS_ClientMP101           = 3,      // Netgear MP101
    CDS_ClientXBox            = 4,      // XBox 360
} UPnpCDSClient;

typedef struct
{
    UPnpCDSClient   nClientType;
    QString         sClientId;
} UPnpCDSClientException;

//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpCDSRequest
{
    public:

        QString           m_sObjectId;

        QString           m_sContainerID;
        QString           m_sFilter;
        short             m_nStartingIndex;
        short             m_nRequestedCount;
        QString           m_sSortCriteria;

        // Browse specific properties

        QString           m_sParentId;
        UPnpCDSBrowseFlag m_eBrowseFlag;

        // Search specific properties

        QString           m_sSearchCriteria;
        QStringList       m_sSearchList;
        QString           m_sSearchClass;

        // The device performing the request
        UPnpCDSClient     m_eClient;
        double            m_nClientVersion;

    public:

        UPnpCDSRequest() : m_nStartingIndex ( 0 ),
                           m_nRequestedCount( 0 ),
                           m_eBrowseFlag( CDS_BrowseUnknown ),
                           m_eClient( CDS_ClientDefault ),
                           m_nClientVersion( 0 )
        {
        }
};

//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpCDSExtensionResults
{
    public:

        CDSObjects              m_List;
        UPnPResultCode          m_eErrorCode;
        QString                 m_sErrorDesc;

        short                   m_nTotalMatches;
        short                   m_nUpdateID;

    public:

        UPnpCDSExtensionResults() : m_eErrorCode( UPnPResult_Success ),
                                    m_nTotalMatches(0), 
                                    m_nUpdateID(0)
        {
        }
        ~UPnpCDSExtensionResults()
        {
            while (!m_List.empty())
            {
                delete m_List.back();
                m_List.pop_back();
            }
        }

        void    Add         ( CDSObject *pObject );
        QString GetResultXML(FilterMap &filter);
};

//////////////////////////////////////////////////////////////////////////////

typedef struct
{
    const char *title;
    const char *column;
    const char *sql;
    const char *where;
    const char *orderColumn;

} UPnpCDSRootInfo;
         
class UPNP_PUBLIC UPnpCDSExtension
{
    public:

        QString     m_sExtensionId;
        QString     m_sName;
        QString     m_sClass;

    protected:

        QString RemoveToken ( const QString &sToken, const QString &sStr, int num );

        virtual UPnpCDSExtensionResults *ProcessRoot     ( UPnpCDSRequest          *pRequest, 
                                                           UPnpCDSExtensionResults *pResults,
                                                           QStringList             &idPath );
        virtual UPnpCDSExtensionResults *ProcessAll      ( UPnpCDSRequest          *pRequest, 
                                                           UPnpCDSExtensionResults *pResults,
                                                           QStringList             &idPath );
        virtual UPnpCDSExtensionResults *ProcessItem     ( UPnpCDSRequest          *pRequest,
                                                           UPnpCDSExtensionResults *pResults,
                                                           QStringList             &idPath );
        virtual UPnpCDSExtensionResults *ProcessKey      ( UPnpCDSRequest          *pRequest,
                                                           UPnpCDSExtensionResults *pResults,
                                                           QStringList             &idPath );
        virtual UPnpCDSExtensionResults *ProcessContainer( UPnpCDSRequest          *pRequest,
                                                           UPnpCDSExtensionResults *pResults,
                                                           int                      nNodeIdx,
                                                           QStringList             &idPath );

        // ------------------------------------------------------------------

        virtual void             CreateItems     ( UPnpCDSRequest          *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   int                      nNodeIdx,
                                                   const QString           &sKey, 
                                                   bool                     bAddRef );

        virtual bool IsBrowseRequestForUs  ( UPnpCDSRequest *pRequest );
        virtual bool IsSearchRequestForUs  ( UPnpCDSRequest *pRequest );

        virtual int  GetDistinctCount      ( UPnpCDSRootInfo *pInfo );
        virtual int  GetCount              ( const QString &sColumn, const QString &sKey );

        // ------------------------------------------------------------------

        virtual UPnpCDSRootInfo *GetRootInfo   ( int nIdx) = 0;
        virtual int              GetRootCount  ( )         = 0;
        virtual QString          GetTableName  ( QString sColumn      ) = 0;
        virtual QString          GetItemListSQL( QString sColumn = "" ) = 0;
        virtual void             BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams ) = 0;

        virtual void       AddItem( const UPnpCDSRequest    *pRequest,
                                    const QString           &sObjectId,
                                    UPnpCDSExtensionResults *pResults,
                                    bool                     bAddRef,
                                    MSqlQuery               &query )  = 0;

        virtual CDSObject *CreateContainer( const QString &sId,
                                            const QString &sTitle,
                                            const QString &sParentId )
        {
            return CDSObject::CreateContainer( sId, sTitle, sParentId );
        }


    public:

        UPnpCDSExtension( QString sName, 
                          QString sExtensionId, 
                          QString sClass )
        {
            m_sName        = QObject::tr(sName.toLatin1().constData());
            m_sExtensionId = sExtensionId;
            m_sClass       = sClass;
        }

        virtual ~UPnpCDSExtension() {}

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSRequest *pRequest );
        virtual UPnpCDSExtensionResults *Search( UPnpCDSRequest *pRequest );

        virtual QString GetSearchCapabilities() { return( "" ); }
        virtual QString GetSortCapabilities  () { return( "" ); }
};

typedef QList<UPnpCDSExtension*> UPnpCDSExtensionList;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnpCDS Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpCDS : public Eventing
{
    private:

        UPnpCDSExtensionList   m_extensions;
        CDSObject              m_root;

        QString                m_sServiceDescFileName;
        QString                m_sControlUrl;

    private:

        UPnpCDSMethod       GetMethod              ( const QString &sURI  );
        UPnpCDSBrowseFlag   GetBrowseFlag          ( const QString &sFlag );

        void            HandleBrowse               ( HTTPRequest *pRequest );
        void            HandleSearch               ( HTTPRequest *pRequest );
        void            HandleGetSearchCapabilities( HTTPRequest *pRequest );
        void            HandleGetSortCapabilities  ( HTTPRequest *pRequest );
        void            HandleGetSystemUpdateID    ( HTTPRequest *pRequest );
        void            DetermineClient            ( HTTPRequest *pRequest, UPnpCDSRequest *pCDSRequest );

    protected:

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceType      () { return "urn:schemas-upnp-org:service:ContentDirectory:1"; }
        virtual QString GetServiceId        () { return "urn:upnp-org:serviceId:CDS_1-0"; }
        virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
        virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
        UPnpCDS( UPnpDevice *pDevice,
                 const QString &sSharePath ); 

        virtual ~UPnpCDS();

        void     RegisterExtension  ( UPnpCDSExtension *pExtension );
        void     UnregisterExtension( UPnpCDSExtension *pExtension );

        virtual bool ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif

// vim:ts=4:sw=4:ai:et:si:sts=4

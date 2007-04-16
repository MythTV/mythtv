//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCDS.h
//                                                                            
// Purpose - 
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDS_H_
#define UPnpCDS_H_

#include <qdom.h>
#include <qdatetime.h> 

#include "upnp.h"
#include "upnpcdsobjects.h"
#include "eventing.h"
              
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

//////////////////////////////////////////////////////////////////////////////

class UPnpCDSRequest
{
    public:

        QString           m_sObjectId;

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

    public:

        UPnpCDSRequest() : m_nStartingIndex ( 0 ),
                           m_nRequestedCount( 0 ),
                           m_eBrowseFlag( CDS_BrowseUnknown )
        {
        }
};

//////////////////////////////////////////////////////////////////////////////

class UPnpCDSExtensionResults
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
            m_List.setAutoDelete( true );
        }

        void    Add         ( CDSObject *pObject );
        QString GetResultXML();
};

//////////////////////////////////////////////////////////////////////////////

class UPnpCDSExtension
{
    public:

        QString     m_sExtensionId;
        QString     m_sName;

    public:

        UPnpCDSExtension( QString sName, QString sExtensionId )
        {
            m_sName        = QObject::tr( sName );
            m_sExtensionId = sExtensionId;
        }

        virtual ~UPnpCDSExtension() {}

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSRequest *pRequest ) = 0;
        virtual UPnpCDSExtensionResults *Search( UPnpCDSRequest *pRequest ) = 0;

        virtual QString GetSearchCapabilities() { return( "" ); }
        virtual QString GetSortCapabilities  () { return( "" ); }
};

typedef QPtrList< UPnpCDSExtension > UPnpCDSExtensionList;   

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnpCDS Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPnpCDS : public Eventing
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

    protected:

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceType      () { return "urn:schemas-upnp-org:service:ContentDirectory:1"; }
        virtual QString GetServiceId        () { return "urn:upnp-org:serviceId:CDS_1-0"; }
        virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
        virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
                 UPnpCDS( UPnpDevice *pDevice ); 
        virtual ~UPnpCDS();

        void     RegisterExtension  ( UPnpCDSExtension *pExtension );
        void     UnregisterExtension( UPnpCDSExtension *pExtension );

        virtual bool ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif

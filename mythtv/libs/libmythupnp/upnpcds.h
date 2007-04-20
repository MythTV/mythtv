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

typedef struct
{
    char *title;
    char *column;
    char *sql;
    char *where;

} UPnpCDSRootInfo;
         
class UPnpCDSExtension
{
    public:

        QString     m_sExtensionId;
        QString     m_sName;
        QString     m_sClass;

    protected:

        int  GetDistinctCount      ( const QString &sColumn );
        int  GetCount              ( const QString &sColumn, const QString &sKey );

        QString RemoveToken ( const QString &sToken, const QString &sStr, int num );

        UPnpCDSExtensionResults *ProcessRoot     ( UPnpCDSRequest          *pRequest, 
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessAll      ( UPnpCDSRequest          *pRequest, 
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessItem     ( UPnpCDSRequest          *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessKey      ( UPnpCDSRequest          *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessContainer( UPnpCDSRequest          *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   int                      nNodeIdx,
                                                   QStringList             &idPath );

        void                     CreateItems     ( UPnpCDSRequest          *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   int                      nNodeIdx,
                                                   const QString           &sKey, 
                                                   bool                     bAddRef );

        // ------------------------------------------------------------------

        virtual UPnpCDSRootInfo *GetRootInfo   ( int nIdx) = 0;
        virtual int              GetRootCount  ( )         = 0;
        virtual QString          GetTableName  ( QString sColumn      ) = 0;
        virtual QString          GetItemListSQL( QString sColumn = "" ) = 0;
        virtual void             BuildItemQuery( MSqlQuery &query, const QStringMap &mapParams ) = 0;

        virtual void       AddItem( const QString           &sObjectId,
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
            m_sName        = QObject::tr( sName );
            m_sExtensionId = sExtensionId;
            m_sClass       = sClass;
        }

        virtual ~UPnpCDSExtension() {}

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSRequest *pRequest );
        virtual UPnpCDSExtensionResults *Search( UPnpCDSRequest *pRequest );

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

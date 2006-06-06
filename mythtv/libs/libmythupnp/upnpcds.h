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

#include "httpserver.h"
#include "mythcontext.h"
#include "upnpcdsobjects.h"
              
class UPnpCDS;
                          
typedef enum 
{
    CDSM_Unknown                = 0,
    CDSM_Browse                 = 1,
    CDSM_Search                 = 2,
    CDSM_GetSearchCapabilities  = 3,
    CDSM_GetSortCapabilities    = 4,
    CDSM_GetSystemUpdateID      = 5

} UPnpCDSMethod;

typedef enum
{
    CDS_BrowseUnknown         = 0,
    CDS_BrowseMetadata        = 1,
    CDS_BrowseDirectChildren  = 2

} UPnpCDSBrowseFlag;

//////////////////////////////////////////////////////////////////////////////

class UPnpCDSBrowseRequest
{
    public:

        QString           m_sParentId;
        QString           m_sObjectId;
        UPnpCDSBrowseFlag m_eBrowseFlag;
        QString           m_sFilter;
        short             m_nStartingIndex;
        short             m_nRequestedCount;
        QString           m_sSortCriteria;

    public:

        UPnpCDSBrowseRequest() : m_eBrowseFlag    ( CDS_BrowseUnknown ),
                                 m_nStartingIndex ( 0 ),
                                 m_nRequestedCount( 0 )
        {
        }
};

//////////////////////////////////////////////////////////////////////////////

class UPnpCDSSearchRequest
{
    public:

        QString     m_sContainerID;
        QString     m_sSearchCriteria;
        QString     m_sFilter;
        short       m_nStartingIndex;
        short       m_nRequestedCount;
        QString     m_sSortCriteria;

    public:

        UPnpCDSSearchRequest() : m_nStartingIndex ( 0 ),
                                 m_nRequestedCount( 0 )
        {
        }
};

//////////////////////////////////////////////////////////////////////////////

class UPnpCDSExtensionResults
{
    public:

        CDSObjects              m_List;
        short                   m_nErrorCode;
        QString                 m_sErrorDesc;

        short                   m_nTotalMatches;
        short                   m_nUpdateID;

    public:

        UPnpCDSExtensionResults() : m_nErrorCode(0),
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

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSBrowseRequest *pRequest ) = 0;
        virtual UPnpCDSExtensionResults *Search( UPnpCDSSearchRequest *pRequest ) = 0;

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

class UPnpCDS : public HttpServerExtension
{
    private:

        short                   m_nSystemUpdateId;

        UPnpCDSExtensionList   m_extensions;
        CDSObject              m_root;

    private:

        UPnpCDSMethod       GetMethod              ( const QString &sURI  );
        UPnpCDSBrowseFlag   GetBrowseFlag          ( const QString &sFlag );

        void            HandleBrowse               ( HTTPRequest *pRequest );
        void            HandleSearch               ( HTTPRequest *pRequest );
        void            HandleGetSearchCapabilities( HTTPRequest *pRequest );
        void            HandleGetSortCapabilities  ( HTTPRequest *pRequest );
        void            HandleGetSystemUpdateID    ( HTTPRequest *pRequest );

        QString        &Encode                     ( QString &sStr );

    public:
                 UPnpCDS(); 
        virtual ~UPnpCDS();

        void     RegisterExtension  ( UPnpCDSExtension *pExtension );
        void     UnregisterExtension( UPnpCDSExtension *pExtension );

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif

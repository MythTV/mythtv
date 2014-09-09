//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCDS.h
// Created     : Oct. 24, 2005
//
// Purpose     : 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDS_H_
#define UPnpCDS_H_

#include <QList>
#include <QMap>
#include <QString>
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
    CDSM_GetSystemUpdateID      = 6,
    CDSM_GetFeatureList         = 7,
    CDSM_GetServiceResetToken   = 8

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
    CDS_ClientSonyDB          = 5,      // Sony Blu-ray players
} UPnpCDSClient;

typedef struct
{
    UPnpCDSClient   nClientType;
    QString         sHeaderKey;
    QString         sHeaderValue;
} UPnpCDSClientException;

//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpCDSRequest
{
    public:

        QString           m_sObjectId;

        QString           m_sContainerID;
        QString           m_sFilter;
        uint16_t          m_nStartingIndex;
        uint16_t          m_nRequestedCount;
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

        uint16_t                m_nTotalMatches;
        uint16_t                m_nUpdateID;

    public:

        UPnpCDSExtensionResults() : m_eErrorCode( UPnPResult_Success ),
                                    m_nTotalMatches(0), 
                                    m_nUpdateID(0)
        {
        }
        ~UPnpCDSExtensionResults()
        {
            while (!m_List.isEmpty())
            {
                m_List.takeLast()->DecrRef();
            }
        }

        void    Add         ( CDSObject *pObject );
        void    Add         ( CDSObjects objects );
        QString GetResultXML(FilterMap &filter);
};

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// UPnpContainerShortcuts Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnPCDSShortcuts : public UPnPFeature
{
  public:
    UPnPCDSShortcuts() : UPnPFeature("CONTAINER_SHORTCUTS", 1) { }

    /**
     * \brief Allowed values for the Container Shortcut feature
     *
     * ContentDirectory Service v4 2014
     * Table E-13  allowedValueListfor the Shortcut Name element
     */
    enum ShortCutType
    {
        MUSIC,
        MUSIC_ALBUMS,
        MUSIC_ARTISTS,
        MUSIC_GENRES,
        MUSIC_PLAYLISTS,
        MUSIC_RECENTLY_ADDED,
        MUSIC_LAST_PLAYED,
        MUSIC_AUDIOBOOKS,
        MUSIC_STATIONS,
        MUSIC_ALL,
        MUSIC_FOLDER_STRUCTURE,

        IMAGES,
        IMAGES_YEARS,
        IMAGES_YEARS_MONTH,
        IMAGES_ALBUM,
        IMAGES_SLIDESHOWS,
        IMAGES_RECENTLY_ADDED,
        IMAGES_LAST_WATCHED,
        IMAGES_ALL,
        IMAGES_FOLDER_STRUCTURE,

        VIDEOS,
        VIDEOS_GENRES,
        VIDEOS_YEARS,
        VIDEOS_YEARS_MONTH,
        VIDEOS_ALBUM,
        VIDEOS_RECENTLY_ADDED,
        VIDEOS_LAST_PLAYED,
        VIDEOS_RECORDINGS,
        VIDEOS_ALL,
        VIDEOS_FOLDER_STRUCTURE,

        FOLDER_STRUCTURE
    };

    bool AddShortCut(ShortCutType type, const QString &objectID);
    QString CreateXML();

  private:
    QString TypeToName(ShortCutType type);
    QMap<ShortCutType, QString> m_shortcuts;
};

typedef QMap<UPnPCDSShortcuts::ShortCutType, QString> CDSShortCutList;

//////////////////////////////////////////////////////////////////////////////

typedef QMap<QString, QString> IDTokenMap;
typedef QPair<QString, QString> IDToken;

class UPNP_PUBLIC UPnpCDSExtension
{
    public:

        QString     m_sExtensionId;
        QString     m_sName;
        QString     m_sClass;

        CDSShortCutList m_shortcuts;

    protected:

        QString RemoveToken ( const QString &sToken, const QString &sStr, int num );

        // ------------------------------------------------------------------

        virtual bool IsBrowseRequestForUs  ( UPnpCDSRequest *pRequest );
        virtual bool IsSearchRequestForUs  ( UPnpCDSRequest *pRequest );

        // ------------------------------------------------------------------

        virtual int  GetRootCount  ( ) { return m_pRoot->GetChildCount(); }
        virtual int  GetRootContainerCount ( )
                                { return m_pRoot->GetChildContainerCount(); }

        virtual void CreateRoot ( );

        virtual bool LoadMetadata ( const UPnpCDSRequest *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     IDTokenMap tokens,
                                     QString currentToken );
        virtual bool LoadChildren ( const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    IDTokenMap tokens,
                                    QString currentToken );

        IDTokenMap TokenizeIDString ( const QString &Id ) const;
        IDToken    GetCurrentToken  ( const QString &Id ) const;

        CDSObject *m_pRoot;

    public:

        UPnpCDSExtension( QString sName, 
                          QString sExtensionId, 
                          QString sClass ) : m_pRoot(NULL)
        {
            m_sName        = QObject::tr(sName.toLatin1().constData());
            m_sExtensionId = sExtensionId;
            m_sClass       = sClass;
        }

        virtual CDSObject *GetRoot ( );

        virtual ~UPnpCDSExtension();

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSRequest *pRequest );
        virtual UPnpCDSExtensionResults *Search( UPnpCDSRequest *pRequest );

        virtual QString         GetSearchCapabilities() { return( "" ); }
        virtual QString         GetSortCapabilities  () { return( "" ); }
        virtual CDSShortCutList GetShortCuts         () { return m_shortcuts; }
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

        UPnPFeatureList        m_features;
        UPnPCDSShortcuts      *m_pShortCuts;

    private:

        UPnpCDSMethod       GetMethod              ( const QString &sURI  );
        UPnpCDSBrowseFlag   GetBrowseFlag          ( const QString &sFlag );

        void            HandleBrowse               ( HTTPRequest *pRequest );
        void            HandleSearch               ( HTTPRequest *pRequest );
        void            HandleGetSearchCapabilities( HTTPRequest *pRequest );
        void            HandleGetSortCapabilities  ( HTTPRequest *pRequest );
        void            HandleGetSystemUpdateID    ( HTTPRequest *pRequest );
        void            HandleGetFeatureList       ( HTTPRequest *pRequest );
        void            HandleGetServiceResetToken ( HTTPRequest *pRequest );
        void            DetermineClient            ( HTTPRequest *pRequest, UPnpCDSRequest *pCDSRequest );

    protected:

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceType      () { return "urn:schemas-upnp-org:service:ContentDirectory:4"; }
        virtual QString GetServiceId        () { return "urn:upnp-org:serviceId:ContentDirectory"; }
        virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
        virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
        UPnpCDS( UPnpDevice *pDevice,
                 const QString &sSharePath ); 

        virtual ~UPnpCDS();

        void     RegisterExtension  ( UPnpCDSExtension *pExtension );
        void     UnregisterExtension( UPnpCDSExtension *pExtension );

        void     RegisterShortCut   ( UPnPCDSShortcuts::ShortCutType type,
                                      const QString &objectID );
        void     RegisterFeature    ( UPnPFeature *feature );

        virtual QStringList GetBasePaths();
        
        virtual bool ProcessRequest( HTTPRequest *pRequest );
};

#endif

// vim:ts=4:sw=4:ai:et:si:sts=4

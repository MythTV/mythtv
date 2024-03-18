//////////////////////////////////////////////////////////////////////////////
// Program Name: UPnpCDS.h
// Created     : Oct. 24, 2005
//
// Purpose     : 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDS_H_
#define UPnpCDS_H_

// C++ headers
#include <utility>

// QT headers
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include "libmythbase/mythdbcon.h"

#include "libmythupnp/eventing.h"
#include "libmythupnp/upnp.h"
#include "libmythupnp/upnpcdsobjects.h"

class UPnpCDS;

enum UPnpCDSMethod : std::uint8_t
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
};

enum UPnpCDSBrowseFlag : std::uint8_t
{
    CDS_BrowseUnknown         = 0,
    CDS_BrowseMetadata        = 1,
    CDS_BrowseDirectChildren  = 2
};

enum UPnpCDSClient : std::uint8_t
{
    CDS_ClientDefault         = 0,      // (no special attention required)
    CDS_ClientWMP             = 1,      // Windows Media Player
    CDS_ClientXBMC            = 2,      // XBMC
    CDS_ClientMP101           = 3,      // Netgear MP101
    CDS_ClientXBox            = 4,      // XBox 360
    CDS_ClientSonyDB          = 5,      // Sony Blu-ray players
};

struct UPnpCDSClientException
{
    UPnpCDSClient   nClientType;
    QString         sHeaderKey;
    QString         sHeaderValue;
};

//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpCDSRequest
{
    public:

        QString           m_sObjectId;

        QString           m_sContainerID;
        QString           m_sFilter;
        uint16_t          m_nStartingIndex  {0};
        uint16_t          m_nRequestedCount {0};
        QString           m_sSortCriteria;

        // Browse specific properties

        QString           m_sParentId;
        UPnpCDSBrowseFlag m_eBrowseFlag     {CDS_BrowseUnknown};

        // Search specific properties

        QString           m_sSearchCriteria;
        QStringList       m_sSearchList;
        QString           m_sSearchClass;

        // The device performing the request
        UPnpCDSClient     m_eClient          {CDS_ClientDefault};
        double            m_nClientVersion   {0.0};

    public:

        UPnpCDSRequest() = default;
};

//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpCDSExtensionResults
{
    public:

        CDSObjects              m_List;
        UPnPResultCode          m_eErrorCode    {UPnPResult_Success};
        QString                 m_sErrorDesc;

        uint16_t                m_nTotalMatches {0};
        uint16_t                m_nUpdateID     {0};

    public:

        UPnpCDSExtensionResults() = default;
        ~UPnpCDSExtensionResults()
        {
            while (!m_List.isEmpty())
            {
                m_List.takeLast()->DecrRef();
            }
        }

        void    Add         ( CDSObject *pObject );
        void    Add         ( const CDSObjects& objects );
        QString GetResultXML(FilterMap &filter, bool ignoreChildren = false);
};

//////////////////////////////////////////////////////////////////////////////

/**
 * \brief Standard UPnP Shortcut feature
 */

class UPNP_PUBLIC UPnPShortcutFeature : public UPnPFeature
{
  public:
    UPnPShortcutFeature() : UPnPFeature("CONTAINER_SHORTCUTS", 1) { }

    /**
     * \brief Allowed values for the Container Shortcut feature
     *
     * ContentDirectory Service v4 2014
     * Table E-13  allowedValueListfor the Shortcut Name element
     */
    enum ShortCutType : std::uint8_t
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
    QString CreateXML() override; // UPnPFeature

  private:
    static QString TypeToName(ShortCutType type);
    QMap<ShortCutType, QString> m_shortcuts;
};

using CDSShortCutList = QMap<UPnPShortcutFeature::ShortCutType, QString>;

//////////////////////////////////////////////////////////////////////////////

using IDTokenMap = QMap<QString, QString>;
using IDToken = QPair<QString, QString>;

class UPNP_PUBLIC UPnpCDSExtension
{
    public:

        QString     m_sExtensionId;
        QString     m_sName;
        QString     m_sClass;

        CDSShortCutList m_shortcuts;

    protected:

        static QString RemoveToken ( const QString &sToken, const QString &sStr, int num );

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
                                    const IDTokenMap& tokens,
                                    const QString& currentToken );
        virtual bool LoadChildren ( const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    const IDTokenMap& tokens,
                                    const QString& currentToken );

        static IDTokenMap TokenizeIDString ( const QString &Id ) ;
        static IDToken    GetCurrentToken  ( const QString &Id ) ;

        static QString    CreateIDString   ( const QString &RequestId,
                                             const QString &Name,
                                             int Value );
        static QString CreateIDString ( const QString &RequestId,
                                        const QString &Name,
                                        const QString &Value );

        CDSObject *m_pRoot {nullptr};

    public:

        UPnpCDSExtension( QString sName,
                          QString sExtensionId, 
                          QString sClass ) :
            m_sExtensionId(std::move(sExtensionId)),
            m_sName(std::move(sName)),
            m_sClass(std::move(sClass)) {}

        virtual CDSObject *GetRoot ( );

        virtual ~UPnpCDSExtension();

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSRequest *pRequest );
        virtual UPnpCDSExtensionResults *Search( UPnpCDSRequest *pRequest );

        virtual QString         GetSearchCapabilities() { return( "" ); }
        virtual QString         GetSortCapabilities  () { return( "" ); }
        virtual CDSShortCutList GetShortCuts         () { return m_shortcuts; }
};

using UPnpCDSExtensionList = QList<UPnpCDSExtension*>;

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
        UPnPShortcutFeature   *m_pShortCuts {nullptr};

    private:

        static UPnpCDSMethod       GetMethod              ( const QString &sURI  );
        static UPnpCDSBrowseFlag   GetBrowseFlag          ( const QString &sFlag );

        void            HandleBrowse               ( HTTPRequest *pRequest );
        void            HandleSearch               ( HTTPRequest *pRequest );
        static void     HandleGetSearchCapabilities( HTTPRequest *pRequest );
        static void     HandleGetSortCapabilities  ( HTTPRequest *pRequest );
        void            HandleGetSystemUpdateID    ( HTTPRequest *pRequest );
        void            HandleGetFeatureList       ( HTTPRequest *pRequest );
        void            HandleGetServiceResetToken ( HTTPRequest *pRequest );
        static void     DetermineClient            ( HTTPRequest *pRequest, UPnpCDSRequest *pCDSRequest );

    protected:

        // Implement UPnpServiceImpl methods that we can

        QString GetServiceType() override // UPnpServiceImpl
            { return "urn:schemas-upnp-org:service:ContentDirectory:4"; }
        QString GetServiceId() override // UPnpServiceImpl
            { return "urn:upnp-org:serviceId:ContentDirectory"; }
        QString GetServiceControlURL() override // UPnpServiceImpl
            { return m_sControlUrl.mid( 1 ); }
        QString GetServiceDescURL() override // UPnpServiceImpl
            { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    public:
        UPnpCDS( UPnpDevice *pDevice,
                 const QString &sSharePath ); 

        ~UPnpCDS() override;

        void     RegisterExtension  ( UPnpCDSExtension *pExtension );
        void     UnregisterExtension( UPnpCDSExtension *pExtension );

        void     RegisterShortCut   ( UPnPShortcutFeature::ShortCutType type,
                                      const QString &objectID );
        void     RegisterFeature    ( UPnPFeature *feature );

        QStringList GetBasePaths() override; // Eventing
        
        bool ProcessRequest( HTTPRequest *pRequest ) override; // Eventing
};

#endif

// vim:ts=4:sw=4:ai:et:si:sts=4

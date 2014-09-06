//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsvideo.h
//                                                                            
// Purpose - UPnP Content Directory Extention for Videos
//                                                                            
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDSVIDEO_H_
#define UPnpCDSVIDEO_H_

#include "mainserver.h"
#include "upnpcds.h"
              
typedef QMap<int, QString> IntMap;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPnpCDSVideo : public UPnpCDSExtension
{
    public:

        UPnpCDSVideo( );
        virtual ~UPnpCDSVideo() {}

    protected:

        virtual bool             IsBrowseRequestForUs( UPnpCDSRequest *pRequest );
        virtual bool             IsSearchRequestForUs( UPnpCDSRequest *pRequest );

        virtual void             CreateRoot ( );

        virtual bool             LoadMetadata( const UPnpCDSRequest *pRequest,
                                                UPnpCDSExtensionResults *pResults,
                                                IDTokenMap tokens,
                                                QString currentToken );
        virtual bool             LoadChildren( const UPnpCDSRequest *pRequest,
                                               UPnpCDSExtensionResults *pResults,
                                               IDTokenMap tokens,
                                               QString currentToken );
    private:
        bool LoadSeries( const UPnpCDSRequest *pRequest,
                     UPnpCDSExtensionResults *pResults,
                     IDTokenMap tokens );

        bool LoadSeasons( const UPnpCDSRequest *pRequest,
                          UPnpCDSExtensionResults *pResults,
                          IDTokenMap tokens );

        bool LoadMovies( const UPnpCDSRequest *pRequest,
                         UPnpCDSExtensionResults *pResults,
                         IDTokenMap tokens );

        bool LoadGenres( const UPnpCDSRequest *pRequest,
                         UPnpCDSExtensionResults *pResults,
                         IDTokenMap tokens );

        bool LoadVideos( const UPnpCDSRequest *pRequest,
                         UPnpCDSExtensionResults *pResults,
                         IDTokenMap tokens );


        void PopulateArtworkURIS( CDSObject *pItem, int nVideoId,
                                  const QUrl &URIBase );

        // Common code helpers
        QString BuildWhereClause( QStringList clauses,
                                  IDTokenMap tokens );
        void    BindValues ( MSqlQuery &query,
                             IDTokenMap tokens );

        QStringMap             m_mapBackendIp;
        QMap<QString, int>     m_mapBackendPort;

        QUrl m_URIBase;

};

#endif

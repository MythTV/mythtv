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
#include "libmythupnp/upnpcds.h"
              
using IntMap = QMap<int, QString>;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPnpCDSVideo : public UPnpCDSExtension
{
    public:

        UPnpCDSVideo( );
        ~UPnpCDSVideo() override = default;

    protected:

        bool IsBrowseRequestForUs( UPnpCDSRequest *pRequest ) override; // UPnpCDSExtension
        bool IsSearchRequestForUs( UPnpCDSRequest *pRequest ) override; // UPnpCDSExtension

        void CreateRoot ( ) override; // UPnpCDSExtension

        bool LoadMetadata( const UPnpCDSRequest *pRequest,
                           UPnpCDSExtensionResults *pResults,
                           const IDTokenMap& tokens,
                           const QString& currentToken ) override; // UPnpCDSExtension
        bool LoadChildren( const UPnpCDSRequest *pRequest,
                           UPnpCDSExtensionResults *pResults,
                           const IDTokenMap& tokens,
                           const QString& currentToken ) override; // UPnpCDSExtension
    private:
        bool LoadSeries( const UPnpCDSRequest *pRequest,
                     UPnpCDSExtensionResults *pResults,
                     const IDTokenMap& tokens );

        bool LoadSeasons( const UPnpCDSRequest *pRequest,
                          UPnpCDSExtensionResults *pResults,
                          const IDTokenMap& tokens );

        bool LoadMovies( const UPnpCDSRequest *pRequest,
                         UPnpCDSExtensionResults *pResults,
                         IDTokenMap tokens );

        static bool LoadGenres( const UPnpCDSRequest *pRequest,
                                UPnpCDSExtensionResults *pResults,
                                const IDTokenMap& tokens );

        bool LoadVideos( const UPnpCDSRequest *pRequest,
                         UPnpCDSExtensionResults *pResults,
                         const IDTokenMap& tokens );


        static void PopulateArtworkURIS( CDSObject *pItem, int nVidID,
                                  const QUrl &URIBase );

        // Common code helpers
        static QString BuildWhereClause( QStringList clauses,
                                  IDTokenMap tokens );
        static void    BindValues ( MSqlQuery &query,
                             IDTokenMap tokens );

        QStringMap             m_mapBackendIp;
        QMap<QString, int>     m_mapBackendPort;

        QUrl m_uriBase;

};

#endif

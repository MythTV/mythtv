//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsmusic.h
//
// Purpose - uPnp Content Directory Extension for Music
//
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDSMusic_H_
#define UPnpCDSMusic_H_

#include <QString>

#include "libmythupnp/upnpcds.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
class MSqlQuery;
class UPnpCDSMusic : public UPnpCDSExtension
{
    public:

        UPnpCDSMusic();
        ~UPnpCDSMusic() override = default;

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

        QUrl             m_uriBase;

        void             PopulateArtworkURIS( CDSObject *pItem,
                                              int songID );

        static bool      LoadArtists(const UPnpCDSRequest *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     const IDTokenMap& tokens);
        bool             LoadAlbums(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    const IDTokenMap& tokens);
        static bool      LoadGenres(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    const IDTokenMap& tokens);
        bool             LoadTracks(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    const IDTokenMap& tokens);

        // Common code helpers
        static QString BuildWhereClause( QStringList clauses,
                                  IDTokenMap tokens );
        static void    BindValues ( MSqlQuery &query,
                             IDTokenMap tokens );
};

#endif

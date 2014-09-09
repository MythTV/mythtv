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

#include "upnpcds.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
class MSqlQuery;
class UPnpCDSMusic : public UPnpCDSExtension
{
    public:

        UPnpCDSMusic();
        virtual ~UPnpCDSMusic() { };

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

        QUrl             m_URIBase;

        void             PopulateArtworkURIS( CDSObject *pItem,
                                              int songID );

        bool             LoadArtists(const UPnpCDSRequest *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     IDTokenMap tokens);
        bool             LoadAlbums(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    IDTokenMap tokens);
        bool             LoadGenres(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    IDTokenMap tokens);
        bool             LoadTracks(const UPnpCDSRequest *pRequest,
                                    UPnpCDSExtensionResults *pResults,
                                    IDTokenMap tokens);

        // Common code helpers
        QString BuildWhereClause( QStringList clauses,
                                  IDTokenMap tokens );
        void    BindValues ( MSqlQuery &query,
                             IDTokenMap tokens );
};

#endif

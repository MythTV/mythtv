//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdstv.h
//
// Purpose - uPnp Content Directory Extension for Recorded TV
//
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDSTV_H_
#define UPnpCDSTV_H_

#include "upnpcds.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPnpCDSTv : public UPnpCDSExtension
{
    public:

        UPnpCDSTv();
        virtual ~UPnpCDSTv() {}

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
        bool  LoadRecordings ( const UPnpCDSRequest *pRequest,
                               UPnpCDSExtensionResults *pResults,
                               IDTokenMap tokens );
        bool  LoadTitles     ( const UPnpCDSRequest *pRequest,
                               UPnpCDSExtensionResults *pResults,
                               IDTokenMap tokens );
        bool  LoadDates      ( const UPnpCDSRequest *pRequest,
                               UPnpCDSExtensionResults *pResults,
                               IDTokenMap tokens );
        bool  LoadGenres     ( const UPnpCDSRequest *pRequest,
                               UPnpCDSExtensionResults *pResults,
                               IDTokenMap tokens );
        bool  LoadChannels   ( const UPnpCDSRequest *pRequest,
                               UPnpCDSExtensionResults *pResults,
                               IDTokenMap tokens );
        bool  LoadRecGroups  ( const UPnpCDSRequest *pRequest,
                               UPnpCDSExtensionResults *pResults,
                               IDTokenMap tokens );
        bool  LoadMovies     ( const UPnpCDSRequest *pRequest,
                               UPnpCDSExtensionResults *pResults,
                               IDTokenMap tokens );
//         bool  LoadSeasons    ( const UPnpCDSRequest *pRequest,
//                                UPnpCDSExtensionResults *pResults,
//                                IDTokenMap tokens );
//         bool  LoadEpisodes   ( const UPnpCDSRequest *pRequest,
//                                UPnpCDSExtensionResults *pResults,
//                                IDTokenMap tokens );

        void PopulateArtworkURIS( CDSObject *pItem, const QString &sInetRef,
                                  int nSeason, const QUrl &URIBase );

        // Common code helpers
        QString BuildWhereClause( QStringList clauses,
                                  IDTokenMap tokens );
        void    BindValues ( MSqlQuery &query,
                             IDTokenMap tokens );

        QUrl                   m_URIBase;

        QStringMap             m_mapBackendIp;
        QMap<QString, int>     m_mapBackendPort;
};

#endif

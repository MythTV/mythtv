//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdstv.h
//                                                                            
// Purpose - uPnp Content Directory Extention for Recorded TV 
//                                                                            
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDSVIDEO_H_
#define UPnpCDSVIDEO_H_

#include "mainserver.h"
#include "upnpcds.h"
              
//using namespace UPnp;

typedef QMap< QString, QString > StringMap;

typedef QMap<int, QString> IntMap;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPnpCDSVideo : public UPnpCDSExtension
{
    private:

        StringMap    m_mapBackendIp;
        StringMap    m_mapBackendPort;

        IntMap       m_mapGenreNames;
        IntMap       m_mapCountryNames;
        IntMap       m_mapGenre;
        IntMap       m_mapCountry;

    private:

        QString RemoveToken ( const QString &sToken, const QString &sStr, int num );

        int  GetDistinctCount      ( const QString &sColumn );
        int  GetCount              ( const QString &sColumn, const QString &sKey );

        void FillMetaMaps (void);

        void BuildContainerChildren( UPnpCDSExtensionResults *pResults, 
                                     int                      nNodeIdx,
                                     const QString           &sParentId,
                                     short                    nStartingIndex,
                                     short                    nRequestedCount );

        void CreateVideoItems      ( UPnpCDSBrowseRequest    *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     int                      nNodeIdx,
                                     const QString           &sKey, 
                                     bool                     bAddRef );

        void AddVideoItem          ( UPnpCDSBrowseRequest    *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     bool                     bAddRef, 
                                     MSqlQuery               &query );

        UPnpCDSExtensionResults *ProcessRoot     ( UPnpCDSBrowseRequest    *pRequest, 
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessAll      ( UPnpCDSBrowseRequest    *pRequest, 
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessItem     ( UPnpCDSBrowseRequest    *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessKey      ( UPnpCDSBrowseRequest    *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessContainer( UPnpCDSBrowseRequest    *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   int                      nNodeIdx,
                                                   QStringList             &idPath );


    public:

        UPnpCDSVideo( ) : UPnpCDSExtension( "Videos", "Videos" )
        {
            FillMetaMaps();
        }

        virtual ~UPnpCDSVideo() {}

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSBrowseRequest *pRequest );
        virtual UPnpCDSExtensionResults *Search( UPnpCDSSearchRequest * /* pRequest */ )
        { 
            return( NULL );
        }

        virtual QString GetSearchCapabilities() { return( "" ); }
        virtual QString GetSortCapabilities  () { return( "" ); }
};

#endif

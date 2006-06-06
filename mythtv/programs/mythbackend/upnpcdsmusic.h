//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdsmusic.h
//                                                                            
// Purpose - uPnp Content Directory Extention for Recorded TV 
//                                                                            
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDSMusic_H_
#define UPnpCDSMusic_H_

#include "mainserver.h"
#include "upnpcds.h"
              
//using namespace UPnp;

typedef QMap< QString, QString > StringMap;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPnpCDSMusic : public UPnpCDSExtension
{
    private:

        QString RemoveToken ( const QString &sToken, const QString &sStr, int num );

        int  GetDistinctCount      ( const QString &sColumn, const QString &sWhere = "" );
        int  GetCount              ( int nNodeIdx, const QString &sKey );

        void BuildContainerChildren( UPnpCDSExtensionResults *pResults, 
                                     int                      nNodeIdx,
                                     const QString           &sParentId,
                                     short                    nStartingIndex,
                                     short                    nRequestedCount );

        void CreateMusicTracks     ( UPnpCDSBrowseRequest    *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     int                      nNodeIdx,
                                     const QString           &sKey, 
                                     bool                     bAddRef );

        void AddMusicTrack         ( UPnpCDSBrowseRequest    *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     bool                     bAddRef, 
                                     int                      nNodeIdx,
                                     MSqlQuery               &query );

        UPnpCDSExtensionResults *ProcessRoot     ( UPnpCDSBrowseRequest    *pRequest, 
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessAll      ( UPnpCDSBrowseRequest    *pRequest, 
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath, 
                                                   int                      nNodeIdx );
        UPnpCDSExtensionResults *ProcessTrack    ( UPnpCDSBrowseRequest    *pRequest,
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

        UPnpCDSMusic( ) : UPnpCDSExtension( "Music", "Music" )
        {
        }

        virtual ~UPnpCDSMusic() {}

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSBrowseRequest *pRequest );
        virtual UPnpCDSExtensionResults *Search( UPnpCDSSearchRequest * /* pRequest */ )
        { 
            return( NULL );
        }

        virtual QString GetSearchCapabilities() { return( "" ); }
        virtual QString GetSortCapabilities  () { return( "" ); }
};

#endif

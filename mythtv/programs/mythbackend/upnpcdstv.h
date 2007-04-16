//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpcdstv.h
//                                                                            
// Purpose - uPnp Content Directory Extention for Recorded TV 
//                                                                            
// Created By  : David Blain                    Created On : Jan. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef UPnpCDSTV_H_
#define UPnpCDSTV_H_

#include "mainserver.h"
#include "upnpcds.h"
              
//using namespace UPnp;

typedef QMap< QString, QString > StringMap;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPnpCDSTv : public UPnpCDSExtension
{
    private:

        StringMap    m_mapBackendIp;
        StringMap    m_mapBackendPort;

    private:

        QString RemoveToken ( const QString &sToken, const QString &sStr, int num );

        int  GetDistinctCount      ( const QString &sColumn );
        int  GetCount              ( const QString &sColumn, const QString &sKey );

        void BuildContainerChildren( UPnpCDSExtensionResults *pResults, 
                                     int                      nNodeIdx,
                                     const QString           &sParentId,
                                     short                    nStartingIndex,
                                     short                    nRequestedCount );

        void CreateVideoItems      ( UPnpCDSRequest          *pRequest,
                                     UPnpCDSExtensionResults *pResults,
                                     int                      nNodeIdx,
                                     const QString           &sKey, 
                                     bool                     bAddRef );

        void AddVideoItem          ( const QString           &sObjectId,
                                     UPnpCDSExtensionResults *pResults,
                                     bool                     bAddRef, 
                                     MSqlQuery               &query );

        UPnpCDSExtensionResults *ProcessRoot     ( UPnpCDSRequest          *pRequest, 
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessAll      ( UPnpCDSRequest          *pRequest, 
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessItem     ( UPnpCDSRequest          *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessKey      ( UPnpCDSRequest          *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   QStringList             &idPath );
        UPnpCDSExtensionResults *ProcessContainer( UPnpCDSRequest          *pRequest,
                                                   UPnpCDSExtensionResults *pResults,
                                                   int                      nNodeIdx,
                                                   QStringList             &idPath );


    public:

        UPnpCDSTv( ) : UPnpCDSExtension( "Recordings", "RecTv" )
        {
        }

        virtual ~UPnpCDSTv() {}

        virtual UPnpCDSExtensionResults *Browse( UPnpCDSRequest *pRequest );
        virtual UPnpCDSExtensionResults *Search( UPnpCDSRequest *pRequest );

        virtual QString GetSearchCapabilities() { return( "" ); }
        virtual QString GetSortCapabilities  () { return( "" ); }
};

#endif

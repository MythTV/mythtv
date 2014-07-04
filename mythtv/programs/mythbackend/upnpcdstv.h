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
    private:

        static UPnpCDSRootInfo g_RootNodes[];
        static int             g_nRootCount;

        QStringMap             m_mapBackendIp;
        QMap<QString, int>     m_mapBackendPort;

    protected:

        virtual bool             IsBrowseRequestForUs( UPnpCDSRequest *pRequest );
        virtual bool             IsSearchRequestForUs( UPnpCDSRequest *pRequest );

        virtual UPnpCDSRootInfo *GetRootInfo   (int nIdx);
        virtual int              GetRootCount  ( );
        virtual QString          GetTableName  ( QString sColumn );
        virtual QString          GetItemListSQL( QString sColumn = "" );

        virtual void             BuildItemQuery( MSqlQuery        &query,
                                                 const QStringMap &mapParams );

        virtual void             AddItem( const UPnpCDSRequest    *pRequest, 
                                          const QString           &sObjectId,
                                          UPnpCDSExtensionResults *pResults,
                                          bool                     bAddRef,
                                          MSqlQuery               &query );

    public:

        UPnpCDSTv( ) : UPnpCDSExtension( "Recordings", "RecTv",
                                         "object.item.videoItem" )
        {
        }

        virtual ~UPnpCDSTv() {}
};

#endif

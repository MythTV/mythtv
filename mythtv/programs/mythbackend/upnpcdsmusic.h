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
    private:

        static UPnpCDSRootInfo g_RootNodes[];
        static int             g_nRootCount;

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

        UPnpCDSMusic( ) : UPnpCDSExtension( "Music", "Music",
                                            "object.item.audioItem.musicTrack" )
        {
        }

        virtual ~UPnpCDSMusic() {}
};

#endif

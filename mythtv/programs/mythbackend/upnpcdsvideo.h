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
        virtual QString          GetItemListSQL( QString sColumn = "");

        virtual void             BuildItemQuery( MSqlQuery        &query, 
                                                 const QStringMap &mapParams );

        virtual void             AddItem( const UPnpCDSRequest    *pRequest, 
                                          const QString           &sObjectId,
                                          UPnpCDSExtensionResults *pResults,
                                          bool                     bAddRef, 
                                          MSqlQuery               &query );

        virtual CDSObject       *CreateContainer( const QString &sId,
                                                  const QString &sTitle,
                                                  const QString &sParentId,
                                                  const QString &sClass );
    public:

        UPnpCDSVideo( ) : UPnpCDSExtension( "Videos", "Videos",
                                            "object.item.videoItem" )
        {
        }
        virtual ~UPnpCDSVideo() {}
};

#endif

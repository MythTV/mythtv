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
        QStringMap             m_mapBackendPort;

        IntMap                 m_mapGenreNames;
        IntMap                 m_mapCountryNames;
        IntMap                 m_mapGenre;
        IntMap                 m_mapCountry;

    protected:

        virtual UPnpCDSRootInfo *GetRootInfo   (int nIdx);
        virtual int              GetRootCount  ( );
        virtual QString          GetTableName  ( QString sColumn );
        virtual QString          GetItemListSQL( QString sColumn = "");

        virtual void             BuildItemQuery( MSqlQuery        &query, 
                                                 const QStringMap &mapParams );

        virtual void             AddItem( const QString           &sObjectId,
                                          UPnpCDSExtensionResults *pResults,
                                          bool                     bAddRef, 
                                          MSqlQuery               &query );

    private:

        void FillMetaMaps (void);

    public:

        UPnpCDSVideo( ) : UPnpCDSExtension( "Videos", "Videos",
                                            "object.item.videoItem" )
        {
            FillMetaMaps();
        }

        virtual ~UPnpCDSVideo() {}
};

#endif

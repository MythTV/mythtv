#ifndef IMAGESERVICES_H_
#define IMAGESERVICES_H_

#include <QFileInfo>
#include <QStringList>

#include "libmythservicecontracts/service.h"

#include "libmythservicecontracts/datacontracts/imageMetadataInfoList.h"
#include "libmythservicecontracts/datacontracts/imageSyncInfo.h"



class SERVICE_PUBLIC ImageServices : public Service
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "2.0" )
    Q_CLASSINFO( "RemoveImage_Method",              "POST" )
    Q_CLASSINFO( "RenameImage_Method",              "POST" )
    Q_CLASSINFO( "StartSync_Method",                "POST" )
    Q_CLASSINFO( "StopSync_Method",                 "POST" )
    Q_CLASSINFO( "CreateThumbnail_Method",          "POST" )

    public:

        // Must call InitializeCustomTypes for each unique
        // Custom Type used in public slots below.
        ImageServices( QObject *parent = nullptr ) : Service( parent )
        {
            // Must call InitializeCustomTypes for each
            // unique Custom Type used in public slots below.
            DTC::ImageMetadataInfoList::InitializeCustomTypes();
            DTC::ImageSyncInfo::InitializeCustomTypes();
        }

    public slots:

        virtual QString                     GetImageInfo       ( int   Id,
                                                                 const QString &Tag ) = 0;

        virtual DTC::ImageMetadataInfoList* GetImageInfoList   ( int   Id ) = 0;

        virtual bool                        RemoveImage        ( int   Id ) = 0;
        virtual bool                        RenameImage        ( int   Id,
                                                                 const QString &NewName ) = 0;

        virtual bool                        StartSync          ( void ) = 0;
        virtual bool                        StopSync           ( void ) = 0;
        virtual DTC::ImageSyncInfo*         GetSyncStatus      ( void ) = 0;

        virtual bool                        CreateThumbnail    ( int  Id ) = 0;
};

#endif

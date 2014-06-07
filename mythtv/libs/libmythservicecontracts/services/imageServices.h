#ifndef IMAGESERVICES_H_
#define IMAGESERVICES_H_

#include <QFileInfo>
#include <QStringList>

#include "service.h"
#include "datacontracts/imageMetadataInfoList.h"
#include "datacontracts/imageSyncInfo.h"



class SERVICE_PUBLIC ImageServices : public Service
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "2.0" )
    Q_CLASSINFO( "SetImageInfo_Method",             "POST" )
    Q_CLASSINFO( "RemoveImageFromDB_Method",        "POST" )
    Q_CLASSINFO( "RemoveImage_Method",              "POST" )
    Q_CLASSINFO( "StartSync_Method",                "POST" )
    Q_CLASSINFO( "StopSync_Method",                 "POST" )
    Q_CLASSINFO( "StartThumbnailGeneration_Method", "POST" )
    Q_CLASSINFO( "StopThumbnailGeneration_Method",  "POST" )
    Q_CLASSINFO( "CreateThumbnail_Method",          "POST" )
    Q_CLASSINFO( "RecreateThumbnail_Method",        "POST" )
    Q_CLASSINFO( "SetThumbnailSize_Method",         "POST" )

    public:

        // Must call InitializeCustomTypes for each unique
        // Custom Type used in public slots below.
        ImageServices( QObject *parent = 0 ) : Service( parent )
        {
            // Must call InitializeCustomTypes for each
            // unique Custom Type used in public slots below.
            DTC::ImageMetadataInfoList::InitializeCustomTypes();
            DTC::ImageSyncInfo::InitializeCustomTypes();
        }

    public slots:

        virtual bool                        SetImageInfo                ( int   Id,
                                                                          const QString &Tag,
                                                                          const QString &Value ) = 0;

        virtual bool                        SetImageInfoByFileName      ( const QString &FileName,
                                                                          const QString &Tag,
                                                                          const QString &Value ) = 0;

        virtual QString                     GetImageInfo                ( int   Id,
                                                                          const QString &Tag ) = 0;

        virtual QString                     GetImageInfoByFileName      ( const QString &FileName,
                                                                          const QString &Tag ) = 0;

        virtual DTC::ImageMetadataInfoList* GetImageInfoList            ( int   Id ) = 0;

        virtual DTC::ImageMetadataInfoList* GetImageInfoListByFileName  ( const QString &FileName ) = 0;

        virtual bool                        RemoveImageFromDB  ( int   Id ) = 0;
        virtual bool                        RemoveImage        ( int   Id ) = 0;

        virtual bool                        StartSync          ( void ) = 0;
        virtual bool                        StopSync           ( void ) = 0;
        virtual DTC::ImageSyncInfo*         GetSyncStatus      ( void ) = 0;

        virtual bool                        StartThumbnailGeneration ( void ) = 0;
        virtual bool                        StopThumbnailGeneration  ( void ) = 0;
        virtual bool                        CreateThumbnail    ( int   Id ) = 0;
        virtual bool                        RecreateThumbnail  ( int   Id ) = 0;
        virtual bool                        SetThumbnailSize   ( int   Width,
                                                                 int   Height) = 0;
};

#endif

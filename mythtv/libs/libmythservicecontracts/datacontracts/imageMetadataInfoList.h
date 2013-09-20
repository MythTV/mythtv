#ifndef IMAGEMETADATAINFOLIST_H
#define IMAGEMETADATAINFOLIST_H

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"
#include "imageMetadataInfo.h"



namespace DTC
{

class SERVICE_PUBLIC ImageMetadataInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" )

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "ImageMetadataInfos", "type=DTC::ImageMetadataInfo")

    Q_PROPERTY( int          Count              READ Count              WRITE setCount          )
    Q_PROPERTY( QString      File               READ File               WRITE setFile           )
    Q_PROPERTY( QString      Path               READ Path               WRITE setPath           )
    Q_PROPERTY( int          Size               READ Size               WRITE setSize           )
    Q_PROPERTY( QString      Extension          READ Extension          WRITE setExtension      )
    Q_PROPERTY( QVariantList ImageMetadataInfos READ ImageMetadataInfos DESIGNABLE true )

    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( QString     , File            )
    PROPERTYIMP       ( QString     , Path            )
    PROPERTYIMP       ( int         , Size            )
    PROPERTYIMP       ( QString     , Extension       )
    PROPERTYIMP_RO_REF( QVariantList, ImageMetadataInfos )

    public:

        static inline void InitializeCustomTypes();

    public:

        ImageMetadataInfoList(QObject *parent = 0)
            : QObject( parent ),
              m_Count         ( 0      ),
              m_Size          ( 0      )
        {
        }

        ImageMetadataInfoList( const ImageMetadataInfoList &src )
        {
            Copy( src );
        }

        void Copy( const ImageMetadataInfoList &src )
        {
            m_Count         = src.m_Count;
            m_File          = src.m_File;
            m_Path          = src.m_Path;
            m_Size          = src.m_Size;
            m_Extension     = src.m_Extension;

            CopyListContents< ImageMetadataInfo >( this, m_ImageMetadataInfos, src.m_ImageMetadataInfos );
        }

        ImageMetadataInfo *AddNewImageMetadataInfo()
        {
            // We must make sure the object added to the
            // QVariantList has a parent of 'this'
            ImageMetadataInfo *pObject = new ImageMetadataInfo( this );
            m_ImageMetadataInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ImageMetadataInfoList  )
Q_DECLARE_METATYPE( DTC::ImageMetadataInfoList* )

namespace DTC
{
inline void ImageMetadataInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< ImageMetadataInfoList  >();
    qRegisterMetaType< ImageMetadataInfoList* >();

    ImageMetadataInfo::InitializeCustomTypes();
}
}

#endif // IMAGEMETADATAINFOLIST_H

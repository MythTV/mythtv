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
    Q_PROPERTY( QVariantList ImageMetadataInfos READ ImageMetadataInfos )

    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP_REF   ( QString     , File            )
    PROPERTYIMP_REF   ( QString     , Path            )
    PROPERTYIMP       ( int         , Size            )
    PROPERTYIMP_REF   ( QString     , Extension       )
    PROPERTYIMP_RO_REF( QVariantList, ImageMetadataInfos )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE ImageMetadataInfoList(QObject *parent = nullptr)
            : QObject( parent ),
              m_Count         ( 0      ),
              m_Size          ( 0      )
        {
        }

        void Copy( const ImageMetadataInfoList *src )
        {
            m_Count         = src->m_Count;
            m_File          = src->m_File;
            m_Path          = src->m_Path;
            m_Size          = src->m_Size;
            m_Extension     = src->m_Extension;

            CopyListContents< ImageMetadataInfo >( this, m_ImageMetadataInfos, src->m_ImageMetadataInfos );
        }

        ImageMetadataInfo *AddNewImageMetadataInfo()
        {
            // We must make sure the object added to the
            // QVariantList has a parent of 'this'
            auto *pObject = new ImageMetadataInfo( this );
            m_ImageMetadataInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(ImageMetadataInfoList);
};

inline void ImageMetadataInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< ImageMetadataInfoList* >();

    ImageMetadataInfo::InitializeCustomTypes();
}

} // namespace DTC

#endif // IMAGEMETADATAINFOLIST_H

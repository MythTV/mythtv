#ifndef IMAGEMETADATAINFO_H
#define IMAGEMETADATAINFO_H

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"



namespace DTC
{

class SERVICE_PUBLIC ImageMetadataInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.00" )

    Q_PROPERTY( int             Number      READ Number         WRITE setNumber     )
    Q_PROPERTY( QString         Family      READ Family         WRITE setFamily     )
    Q_PROPERTY( QString         Group       READ Group          WRITE setGroup      )
    Q_PROPERTY( QString         Tag         READ Tag            WRITE setTag        )
    Q_PROPERTY( QString         Key         READ Key            WRITE setKey        )
    Q_PROPERTY( QString         Label       READ Label          WRITE setLabel      )
    Q_PROPERTY( QString         Value       READ Value          WRITE setValue      )

    PROPERTYIMP    ( int        , Number       )
    PROPERTYIMP    ( QString    , Family       )
    PROPERTYIMP    ( QString    , Group        )
    PROPERTYIMP    ( QString    , Tag          )
    PROPERTYIMP    ( QString    , Key          )
    PROPERTYIMP    ( QString    , Label        )
    PROPERTYIMP    ( QString    , Value        )

    public:

        static inline void InitializeCustomTypes();

    public:

        ImageMetadataInfo(QObject *parent = 0)
                        : QObject         ( parent ),
                          m_Number        ( 0      )
        {
        }

        ImageMetadataInfo( const ImageMetadataInfo &src )
        {
            Copy( src );
        }

        void Copy( const ImageMetadataInfo &src )
        {
            m_Number    = src.m_Number;
            m_Family    = src.m_Family;
            m_Group     = src.m_Group;
            m_Tag       = src.m_Tag;
            m_Key       = src.m_Key;
            m_Label     = src.m_Label;
            m_Value     = src.m_Value;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ImageMetadataInfo  )
Q_DECLARE_METATYPE( DTC::ImageMetadataInfo* )

namespace DTC
{
inline void ImageMetadataInfo::InitializeCustomTypes()
{
    qRegisterMetaType< ImageMetadataInfo  >();
    qRegisterMetaType< ImageMetadataInfo* >();
}
}

#endif // IMAGEMETADATAINFO_H

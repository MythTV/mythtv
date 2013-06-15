#ifndef VIDEOMULTIPLEXLIST_H_
#define VIDEOMULTIPLEXLIST_H_

#include <QVariantList>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "videoMultiplex.h"

namespace DTC
{

class SERVICE_PUBLIC VideoMultiplexList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "VideoMultiplexes", "type=DTC::VideoMultiplex");
	Q_CLASSINFO( "AsOf"	           , "transient=true"          );

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          CurrentPage    READ CurrentPage     WRITE setCurrentPage    )
    Q_PROPERTY( int          TotalPages     READ TotalPages      WRITE setTotalPages     )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList VideoMultiplexes READ VideoMultiplexes DESIGNABLE true )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , CurrentPage     )
    PROPERTYIMP       ( int         , TotalPages      )
    PROPERTYIMP       ( int         , TotalAvailable  )
    PROPERTYIMP       ( QDateTime   , AsOf            )
    PROPERTYIMP       ( QString     , Version         )
    PROPERTYIMP       ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, VideoMultiplexes )

    public:

        static inline void InitializeCustomTypes();

    public:

        VideoMultiplexList(QObject *parent = 0)
            : QObject( parent ),
              m_StartIndex      ( 0       ),
              m_Count           ( 0       ),
              m_CurrentPage     ( 0       ),
              m_TotalPages      ( 0       ),
              m_TotalAvailable  ( 0       )
        {
        }

        VideoMultiplexList( const VideoMultiplexList &src )
        {
            Copy( src );
        }

        void Copy( const VideoMultiplexList &src )
        {
            m_AsOf          = src.m_AsOf           ;
            m_Version       = src.m_Version        ;
            m_ProtoVer      = src.m_ProtoVer       ;

            CopyListContents< VideoMultiplex >( this, m_VideoMultiplexes, src.m_VideoMultiplexes );
        }

        VideoMultiplex *AddNewVideoMultiplex()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            VideoMultiplex *pObject = new VideoMultiplex( this );
            m_VideoMultiplexes.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VideoMultiplexList  )
Q_DECLARE_METATYPE( DTC::VideoMultiplexList* )

namespace DTC
{
inline void VideoMultiplexList::InitializeCustomTypes()
{
    qRegisterMetaType< VideoMultiplexList   >();
    qRegisterMetaType< VideoMultiplexList*  >();

    VideoMultiplex::InitializeCustomTypes();
}
}

#endif

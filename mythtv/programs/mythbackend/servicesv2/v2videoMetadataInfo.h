#ifndef V2VIDEOMETADATAINFO_H_
#define V2VIDEOMETADATAINFO_H_

#include "libmythbase/http/mythhttpservice.h"
#include "v2artworkInfoList.h"
#include "v2castMemberList.h"
#include "v2genreList.h"

class V2VideoMetadataInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "2.01" );

    SERVICE_PROPERTY2( int        , Id             )
    SERVICE_PROPERTY2( QString    , Title          )
    SERVICE_PROPERTY2( QString    , SubTitle       )
    SERVICE_PROPERTY2( QString    , Tagline        )
    SERVICE_PROPERTY2( QString    , Director       )
    SERVICE_PROPERTY2( QString    , Studio         )
    SERVICE_PROPERTY2( QString    , Description    )
    SERVICE_PROPERTY2( QString    , Certification  )
    SERVICE_PROPERTY2( QString    , Inetref        )
    SERVICE_PROPERTY2( int        , Collectionref  )
    SERVICE_PROPERTY2( QString    , HomePage       )
    SERVICE_PROPERTY2( QDateTime  , ReleaseDate    )
    SERVICE_PROPERTY2( QDateTime  , AddDate        )
    SERVICE_PROPERTY2( float      , UserRating     )
    SERVICE_PROPERTY2( int        , ChildID        )
    SERVICE_PROPERTY2( int        , Length         )
    SERVICE_PROPERTY2( int        , PlayCount      )
    SERVICE_PROPERTY2( int        , Season         )
    SERVICE_PROPERTY2( int        , Episode        )
    SERVICE_PROPERTY2( int        , ParentalLevel  )
    SERVICE_PROPERTY2( bool       , Visible        )
    SERVICE_PROPERTY2( bool       , Watched        )
    SERVICE_PROPERTY2( bool       , Processed      )
    SERVICE_PROPERTY2( QString    , ContentType    )
    SERVICE_PROPERTY2( QString    , FileName       )
    SERVICE_PROPERTY2( QString    , Hash           )
    SERVICE_PROPERTY2( QString    , HostName       )
    SERVICE_PROPERTY2( QString    , Coverart       )
    SERVICE_PROPERTY2( QString    , Fanart         )
    SERVICE_PROPERTY2( QString    , Banner         )
    SERVICE_PROPERTY2( QString    , Screenshot     )
    SERVICE_PROPERTY2( QString    , Trailer        )
    SERVICE_PROPERTY2( int        , Category       )
    Q_PROPERTY( QObject*        Artwork         READ Artwork     USER true)
    Q_PROPERTY( QObject*        Cast            READ Cast        USER true)
    Q_PROPERTY( QObject*        Genres          READ Genres      USER true)

    SERVICE_PROPERTY_PTR( V2ArtworkInfoList , Artwork   )
    SERVICE_PROPERTY_PTR( V2CastMemberList  , Cast      )
    SERVICE_PROPERTY_PTR( V2GenreList       , Genres    )

  public:
    Q_INVOKABLE V2VideoMetadataInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoMetadataInfo *src )
        {
            m_Id               = src->m_Id;

            if ( src->m_Artwork != nullptr)
                Artwork()->Copy( src->m_Artwork );

            if (src->m_Cast != nullptr)
                Cast()->Copy( src->m_Cast );

            if (src->m_Genres != nullptr)
                Genres()->Copy( src->m_Genres );

        }

    private:
        Q_DISABLE_COPY(V2VideoMetadataInfo);


};

Q_DECLARE_METATYPE(V2VideoMetadataInfo*)

/////////////////////////////////////////////////////////////////////////////

class V2VideoCategory : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    SERVICE_PROPERTY2( int        , Id      )
    SERVICE_PROPERTY2( QString    , Name    )

    public:

        Q_INVOKABLE V2VideoCategory(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    private:
        Q_DISABLE_COPY(V2VideoCategory);
};

Q_DECLARE_METATYPE(V2VideoCategory*)

class V2VideoCategoryList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "VideoCategories", "type=V2VideoCategory");

    SERVICE_PROPERTY2( QVariantList, VideoCategories );

    public:

        Q_INVOKABLE V2VideoCategoryList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        V2VideoCategory *AddNewCategory()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2VideoCategory( this );
            m_VideoCategories.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2VideoCategoryList);
};

Q_DECLARE_METATYPE(V2VideoCategoryList*)


#endif // V2VIDEOMETADATAINFO_H_

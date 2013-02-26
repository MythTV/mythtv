#ifndef CHANNELINFOLIST_H_
#define CHANNELINFOLIST_H_

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "programAndChannel.h"

namespace DTC
{

class SERVICE_PUBLIC ChannelInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "ChannelInfos", "type=DTC::ChannelInfo");
	Q_CLASSINFO( "AsOf"		   , "transient=true"       );

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          CurrentPage    READ CurrentPage     WRITE setCurrentPage    )
    Q_PROPERTY( int          TotalPages     READ TotalPages      WRITE setTotalPages     )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList ChannelInfos READ ChannelInfos DESIGNABLE true )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , CurrentPage     )
    PROPERTYIMP       ( int         , TotalPages      )
    PROPERTYIMP       ( int         , TotalAvailable  )
    PROPERTYIMP       ( QDateTime   , AsOf            )
    PROPERTYIMP       ( QString     , Version         )
    PROPERTYIMP       ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, ChannelInfos )

    public:

        static void InitializeCustomTypes();

    public:

        ChannelInfoList(QObject *parent = 0)
            : QObject( parent ),
              m_StartIndex    ( 0      ),
              m_Count         ( 0      ),
              m_TotalAvailable( 0      )
        {
        }

        ChannelInfoList( const ChannelInfoList &src )
        {
            Copy( src );
        }

        void Copy( const ChannelInfoList &src )
        {
            m_StartIndex    = src.m_StartIndex     ;
            m_Count         = src.m_Count          ;
            m_TotalAvailable= src.m_TotalAvailable ;
            m_AsOf          = src.m_AsOf           ;
            m_Version       = src.m_Version        ;
            m_ProtoVer      = src.m_ProtoVer       ;

            CopyListContents< ChannelInfo >( this, m_ChannelInfos, src.m_ChannelInfos );
        }

        ChannelInfo *AddNewChannelInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            ChannelInfo *pObject = new ChannelInfo( this );
            m_ChannelInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ChannelInfoList  )
Q_DECLARE_METATYPE( DTC::ChannelInfoList* )

namespace DTC
{
inline void ChannelInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< ChannelInfoList   >();
    qRegisterMetaType< ChannelInfoList*  >();

    ChannelInfo::InitializeCustomTypes();
}
}

#endif

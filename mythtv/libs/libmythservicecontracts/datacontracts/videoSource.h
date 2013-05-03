#ifndef VIDEOSOURCE_H_
#define VIDEOSOURCE_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC VideoSource : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( QString         SourceName      READ SourceName       WRITE setSourceName     )
    Q_PROPERTY( QString         Grabber         READ Grabber          WRITE setGrabber        )
    Q_PROPERTY( QString         UserId          READ UserId           WRITE setUserId         )
    Q_PROPERTY( QString         FreqTable       READ FreqTable        WRITE setFreqTable      )
    Q_PROPERTY( QString         LineupId        READ LineupId         WRITE setLineupId       )
    Q_PROPERTY( QString         Password        READ Password         WRITE setPassword       )
    Q_PROPERTY( bool            UseEIT          READ UseEIT           WRITE setUseEIT         )
    Q_PROPERTY( QString         ConfigPath      READ ConfigPath       WRITE setConfigPath     )
    Q_PROPERTY( int             NITId           READ NITId            WRITE setNITId          )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP    ( QString    , SourceName     )
    PROPERTYIMP    ( QString    , Grabber        )
    PROPERTYIMP    ( QString    , UserId         )
    PROPERTYIMP    ( QString    , FreqTable      )
    PROPERTYIMP    ( QString    , LineupId       )
    PROPERTYIMP    ( QString    , Password       )
    PROPERTYIMP    ( bool       , UseEIT         )
    PROPERTYIMP    ( QString    , ConfigPath     )
    PROPERTYIMP    ( int        , NITId          )

    public:

        static inline void InitializeCustomTypes();

    public:

        VideoSource(QObject *parent = 0)
            : QObject         ( parent ),
              m_Id            ( 0      )
        {
        }

        VideoSource( const VideoSource &src )
        {
            Copy( src );
        }

        void Copy( const VideoSource &src )
        {
            m_Id            = src.m_Id            ;
            m_SourceName    = src.m_SourceName    ;
            m_Grabber       = src.m_Grabber       ;
            m_UserId        = src.m_UserId        ;
            m_FreqTable     = src.m_FreqTable     ;
            m_LineupId      = src.m_LineupId      ;
            m_Password      = src.m_Password      ;
            m_UseEIT        = src.m_UseEIT        ;
            m_ConfigPath    = src.m_ConfigPath    ;
            m_NITId         = src.m_NITId         ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VideoSource  )
Q_DECLARE_METATYPE( DTC::VideoSource* )

namespace DTC
{
inline void VideoSource::InitializeCustomTypes()
{
    qRegisterMetaType< VideoSource   >();
    qRegisterMetaType< VideoSource*  >();
}
}

#endif

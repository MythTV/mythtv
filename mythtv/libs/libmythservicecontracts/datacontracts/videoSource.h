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
    Q_PROPERTY( uint            BouquetId       READ BouquetId        WRITE setBouquetId      )
    Q_PROPERTY( uint            RegionId        READ RegionId         WRITE setRegionId       )
    Q_PROPERTY( uint            ScanFrequency   READ ScanFrequency    WRITE setScanFrequency  )
    Q_PROPERTY( uint            LCNOffset       READ LCNOffset        WRITE setLCNOffset      )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP_REF( QString    , SourceName     )
    PROPERTYIMP_REF( QString    , Grabber        )
    PROPERTYIMP_REF( QString    , UserId         )
    PROPERTYIMP_REF( QString    , FreqTable      )
    PROPERTYIMP_REF( QString    , LineupId       )
    PROPERTYIMP_REF( QString    , Password       )
    PROPERTYIMP    ( bool       , UseEIT         )
    PROPERTYIMP_REF( QString    , ConfigPath     )
    PROPERTYIMP    ( int        , NITId          )
    PROPERTYIMP    ( uint       , BouquetId      )
    PROPERTYIMP    ( uint       , RegionId       )
    PROPERTYIMP    ( uint       , ScanFrequency  )
    PROPERTYIMP    ( uint       , LCNOffset      )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE VideoSource(QObject *parent = nullptr)
            : QObject         ( parent ),
              m_Id            ( 0      ),
              m_UseEIT        ( false  ),
              m_NITId         ( 0      ),
              m_BouquetId     ( 0      ),
              m_RegionId      ( 0      ),
              m_ScanFrequency ( 0      )
        {
        }

        void Copy( const VideoSource *src )
        {
            m_Id            = src->m_Id            ;
            m_SourceName    = src->m_SourceName    ;
            m_Grabber       = src->m_Grabber       ;
            m_UserId        = src->m_UserId        ;
            m_FreqTable     = src->m_FreqTable     ;
            m_LineupId      = src->m_LineupId      ;
            m_Password      = src->m_Password      ;
            m_UseEIT        = src->m_UseEIT        ;
            m_ConfigPath    = src->m_ConfigPath    ;
            m_NITId         = src->m_NITId         ;
            m_BouquetId     = src->m_BouquetId     ;
            m_RegionId      = src->m_RegionId      ;
            m_ScanFrequency = src->m_ScanFrequency ;
        }

    private:
        Q_DISABLE_COPY(VideoSource);
};

inline void VideoSource::InitializeCustomTypes()
{
    qRegisterMetaType< VideoSource*  >();
}

} // namespace DTC

#endif

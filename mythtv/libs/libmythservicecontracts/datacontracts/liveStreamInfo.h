#ifndef LIVESTREAMINFO_H_
#define LIVESTREAMINFO_H_

#include <QDateTime>
#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC LiveStreamInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( int             Id               READ Id                WRITE setId               )
    Q_PROPERTY( int             Width            READ Width             WRITE setWidth            )
    Q_PROPERTY( int             Height           READ Height            WRITE setHeight           )
    Q_PROPERTY( int             Bitrate          READ Bitrate           WRITE setBitrate          )
    Q_PROPERTY( int             AudioBitrate     READ AudioBitrate      WRITE setAudioBitrate     )
    Q_PROPERTY( int             SegmentSize      READ SegmentSize       WRITE setSegmentSize      )
    Q_PROPERTY( int             MaxSegments      READ MaxSegments       WRITE setMaxSegments      )
    Q_PROPERTY( int             StartSegment     READ StartSegment      WRITE setStartSegment     )
    Q_PROPERTY( int             CurrentSegment   READ CurrentSegment    WRITE setCurrentSegment   )
    Q_PROPERTY( int             SegmentCount     READ SegmentCount      WRITE setSegmentCount     )
    Q_PROPERTY( int             PercentComplete  READ PercentComplete   WRITE setPercentComplete  )
    Q_PROPERTY( QDateTime       Created          READ Created           WRITE setCreated          )
    Q_PROPERTY( QDateTime       LastModified     READ LastModified      WRITE setLastModified     )
    Q_PROPERTY( QString         RelativeURL      READ RelativeURL       WRITE setRelativeURL      )
    Q_PROPERTY( QString         FullURL          READ FullURL           WRITE setFullURL          )
    Q_PROPERTY( QString         StatusStr        READ StatusStr         WRITE setStatusStr        )
    Q_PROPERTY( int             StatusInt        READ StatusInt         WRITE setStatusInt        )
    Q_PROPERTY( QString         StatusMessage    READ StatusMessage     WRITE setStatusMessage    )
    Q_PROPERTY( QString         SourceFile       READ SourceFile        WRITE setSourceFile       )
    Q_PROPERTY( QString         SourceHost       READ SourceHost        WRITE setSourceHost       )
    Q_PROPERTY( int             SourceWidth      READ SourceWidth       WRITE setSourceWidth      )
    Q_PROPERTY( int             SourceHeight     READ SourceHeight      WRITE setSourceHeight     )
    Q_PROPERTY( int             AudioOnlyBitrate READ AudioOnlyBitrate  WRITE setAudioOnlyBitrate )

    PROPERTYIMP    ( int        , Id               )
    PROPERTYIMP    ( int        , Width            )
    PROPERTYIMP    ( int        , Height           )
    PROPERTYIMP    ( int        , Bitrate          )
    PROPERTYIMP    ( int        , AudioBitrate     )
    PROPERTYIMP    ( int        , SegmentSize      )
    PROPERTYIMP    ( int        , MaxSegments      )
    PROPERTYIMP    ( int        , StartSegment     )
    PROPERTYIMP    ( int        , CurrentSegment   )
    PROPERTYIMP    ( int        , SegmentCount     )
    PROPERTYIMP    ( int        , PercentComplete  )
    PROPERTYIMP    ( QDateTime  , Created          )
    PROPERTYIMP    ( QDateTime  , LastModified     )
    PROPERTYIMP    ( QString    , RelativeURL      )
    PROPERTYIMP    ( QString    , FullURL          )
    PROPERTYIMP    ( QString    , StatusStr        )
    PROPERTYIMP    ( int        , StatusInt        )
    PROPERTYIMP    ( QString    , StatusMessage    )
    PROPERTYIMP    ( QString    , SourceFile       )
    PROPERTYIMP    ( QString    , SourceHost       )
    PROPERTYIMP    ( int        , SourceWidth      )
    PROPERTYIMP    ( int        , SourceHeight     )
    PROPERTYIMP    ( int        , AudioOnlyBitrate )

    public:

        static inline void InitializeCustomTypes();

    public:

        LiveStreamInfo(QObject *parent = 0) 
            : QObject            ( parent ),
              m_Id               ( 0      ),
              m_Width            ( 0      ),
              m_Height           ( 0      ),
              m_Bitrate          ( 0      ),
              m_AudioBitrate     ( 0      ),
              m_SegmentSize      ( 0      ),
              m_MaxSegments      ( 0      ),
              m_StartSegment     ( 0      ),
              m_CurrentSegment   ( 0      ),
              m_SegmentCount     ( 0      ),
              m_PercentComplete  ( 0      ),
              m_StatusInt        ( 0      ),
              m_SourceWidth      ( 0      ),
              m_SourceHeight     ( 0      ),
              m_AudioOnlyBitrate ( 0      )
        { 
        }
        
        LiveStreamInfo( const LiveStreamInfo &src )
        {
            Copy( src );
        }

        void Copy( const LiveStreamInfo &src )
        {
            m_Id                = src.m_Id                ;
            m_Width             = src.m_Width             ;
            m_Height            = src.m_Height            ;
            m_Bitrate           = src.m_Bitrate           ;
            m_AudioBitrate      = src.m_AudioBitrate      ;
            m_SegmentSize       = src.m_SegmentSize       ;
            m_MaxSegments       = src.m_MaxSegments       ;
            m_StartSegment      = src.m_StartSegment      ;
            m_CurrentSegment    = src.m_CurrentSegment    ;
            m_SegmentCount      = src.m_SegmentCount      ;
            m_PercentComplete   = src.m_PercentComplete   ;
            m_Created           = src.m_Created           ;
            m_LastModified      = src.m_LastModified      ;
            m_RelativeURL       = src.m_RelativeURL       ;
            m_FullURL           = src.m_FullURL           ;
            m_StatusStr         = src.m_StatusStr         ;
            m_StatusInt         = src.m_StatusInt         ;
            m_StatusMessage     = src.m_StatusMessage     ;
            m_SourceFile        = src.m_SourceFile        ;
            m_SourceHost        = src.m_SourceHost        ;
            m_SourceWidth       = src.m_SourceWidth       ;
            m_SourceHeight      = src.m_SourceHeight      ;
            m_AudioOnlyBitrate  = src.m_AudioOnlyBitrate  ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::LiveStreamInfo  )
Q_DECLARE_METATYPE( DTC::LiveStreamInfo* )

namespace DTC
{
inline void LiveStreamInfo::InitializeCustomTypes()
{
    qRegisterMetaType< LiveStreamInfo   >();
    qRegisterMetaType< LiveStreamInfo*  >();
}
}

#endif

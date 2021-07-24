#ifndef V2LIVESTREAMINFO_H_
#define V2LIVESTREAMINFO_H_

#include <QDateTime>
#include <QString>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class V2LiveStreamInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    SERVICE_PROPERTY2( int        , Id               )
    SERVICE_PROPERTY2( int        , Width            )
    SERVICE_PROPERTY2( int        , Height           )
    SERVICE_PROPERTY2( int        , Bitrate          )
    SERVICE_PROPERTY2( int        , AudioBitrate     )
    SERVICE_PROPERTY2( int        , SegmentSize      )
    SERVICE_PROPERTY2( int        , MaxSegments      )
    SERVICE_PROPERTY2( int        , StartSegment     )
    SERVICE_PROPERTY2( int        , CurrentSegment   )
    SERVICE_PROPERTY2( int        , SegmentCount     )
    SERVICE_PROPERTY2( int        , PercentComplete  )
    SERVICE_PROPERTY2( QDateTime  , Created          )
    SERVICE_PROPERTY2( QDateTime  , LastModified     )
    SERVICE_PROPERTY2( QString    , RelativeURL      )
    SERVICE_PROPERTY2( QString    , FullURL          )
    SERVICE_PROPERTY2( QString    , StatusStr        )
    SERVICE_PROPERTY2( int        , StatusInt        )
    SERVICE_PROPERTY2( QString    , StatusMessage    )
    SERVICE_PROPERTY2( QString    , SourceFile       )
    SERVICE_PROPERTY2( QString    , SourceHost       )
    SERVICE_PROPERTY2( int        , SourceWidth      )
    SERVICE_PROPERTY2( int        , SourceHeight     )
    SERVICE_PROPERTY2( int        , AudioOnlyBitrate );

    public:

        explicit V2LiveStreamInfo(QObject *parent = nullptr)
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

        void Copy( const V2LiveStreamInfo *src )
        {
            m_Id                = src->m_Id                ;
            m_Width             = src->m_Width             ;
            m_Height            = src->m_Height            ;
            m_Bitrate           = src->m_Bitrate           ;
            m_AudioBitrate      = src->m_AudioBitrate      ;
            m_SegmentSize       = src->m_SegmentSize       ;
            m_MaxSegments       = src->m_MaxSegments       ;
            m_StartSegment      = src->m_StartSegment      ;
            m_CurrentSegment    = src->m_CurrentSegment    ;
            m_SegmentCount      = src->m_SegmentCount      ;
            m_PercentComplete   = src->m_PercentComplete   ;
            m_Created           = src->m_Created           ;
            m_LastModified      = src->m_LastModified      ;
            m_RelativeURL       = src->m_RelativeURL       ;
            m_FullURL           = src->m_FullURL           ;
            m_StatusStr         = src->m_StatusStr         ;
            m_StatusInt         = src->m_StatusInt         ;
            m_StatusMessage     = src->m_StatusMessage     ;
            m_SourceFile        = src->m_SourceFile        ;
            m_SourceHost        = src->m_SourceHost        ;
            m_SourceWidth       = src->m_SourceWidth       ;
            m_SourceHeight      = src->m_SourceHeight      ;
            m_AudioOnlyBitrate  = src->m_AudioOnlyBitrate  ;
        }

    private:
        Q_DISABLE_COPY(V2LiveStreamInfo);
};

Q_DECLARE_METATYPE(V2LiveStreamInfo*)

#endif

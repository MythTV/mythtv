#ifndef V2VIDEOSOURCE_H_
#define V2VIDEOSOURCE_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2VideoSource : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( int        , Id             )
    SERVICE_PROPERTY2( QString    , SourceName     )
    SERVICE_PROPERTY2( QString    , Grabber        )
    SERVICE_PROPERTY2( QString    , UserId         )
    SERVICE_PROPERTY2( QString    , FreqTable      )
    SERVICE_PROPERTY2( QString    , LineupId       )
    SERVICE_PROPERTY2( QString    , Password       )
    SERVICE_PROPERTY2( bool       , UseEIT         )
    SERVICE_PROPERTY2( QString    , ConfigPath     )
    SERVICE_PROPERTY2( int        , NITId          )
    SERVICE_PROPERTY2( uint       , BouquetId      )
    SERVICE_PROPERTY2( uint       , RegionId       )
    SERVICE_PROPERTY2( uint       , ScanFrequency  )
    SERVICE_PROPERTY2( uint       , LCNOffset      )

    public:

        Q_INVOKABLE V2VideoSource(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoSource *src )
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
        Q_DISABLE_COPY(V2VideoSource);
};

Q_DECLARE_METATYPE(V2VideoSource*)

#endif

#ifndef RECRULE_H_
#define RECRULE_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC RecRule : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.11" );

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( int             ParentId        READ ParentId         WRITE setParentId       )
    Q_PROPERTY( bool            Inactive        READ Inactive         WRITE setInactive       )
    Q_PROPERTY( QString         Title           READ Title            WRITE setTitle          )
    Q_PROPERTY( QString         SubTitle        READ SubTitle         WRITE setSubTitle       )
    Q_PROPERTY( QString         Description     READ Description      WRITE setDescription    )
    Q_PROPERTY( uint            Season          READ Season           WRITE setSeason         )
    Q_PROPERTY( uint            Episode         READ Episode          WRITE setEpisode        )
    Q_PROPERTY( QString         Category        READ Category         WRITE setCategory       )

    Q_PROPERTY( QDateTime       StartTime       READ StartTime        WRITE setStartTime      )
    Q_PROPERTY( QDateTime       EndTime         READ EndTime          WRITE setEndTime        )

    Q_PROPERTY( QString         SeriesId        READ SeriesId         WRITE setSeriesId       )
    Q_PROPERTY( QString         ProgramId       READ ProgramId        WRITE setProgramId      )
    Q_PROPERTY( QString         Inetref         READ Inetref          WRITE setInetref        )

    Q_PROPERTY( int             ChanId          READ ChanId           WRITE setChanId         )
    Q_PROPERTY( QString         CallSign        READ CallSign         WRITE setCallSign       )
    Q_PROPERTY( int             FindDay         READ FindDay          WRITE setFindDay        )
    Q_PROPERTY( QTime           FindTime        READ FindTime         WRITE setFindTime       )
    Q_PROPERTY( QString         Type            READ Type             WRITE setType           )
    Q_PROPERTY( QString         SearchType      READ SearchType       WRITE setSearchType     )
    Q_PROPERTY( int             RecPriority     READ RecPriority      WRITE setRecPriority    )
    Q_PROPERTY( uint            PreferredInput  READ PreferredInput   WRITE setPreferredInput )
    Q_PROPERTY( int             StartOffset     READ StartOffset      WRITE setStartOffset    )
    Q_PROPERTY( int             EndOffset       READ EndOffset        WRITE setEndOffset      )
    Q_PROPERTY( QString         DupMethod       READ DupMethod        WRITE setDupMethod      )
    Q_PROPERTY( QString         DupIn           READ DupIn            WRITE setDupIn          )
    Q_PROPERTY( uint            Filter          READ Filter           WRITE setFilter         )

    Q_PROPERTY( QString         RecProfile      READ RecProfile       WRITE setRecProfile     )
    Q_PROPERTY( QString         RecGroup        READ RecGroup         WRITE setRecGroup       )
    Q_PROPERTY( QString         StorageGroup    READ StorageGroup     WRITE setStorageGroup   )
    Q_PROPERTY( QString         PlayGroup       READ PlayGroup        WRITE setPlayGroup      )

    Q_PROPERTY( bool            AutoExpire      READ AutoExpire       WRITE setAutoExpire     )
    Q_PROPERTY( int             MaxEpisodes     READ MaxEpisodes      WRITE setMaxEpisodes    )
    Q_PROPERTY( bool            MaxNewest       READ MaxNewest        WRITE setMaxNewest      )

    Q_PROPERTY( bool            AutoCommflag    READ AutoCommflag     WRITE setAutoCommflag   )
    Q_PROPERTY( bool            AutoTranscode   READ AutoTranscode    WRITE setAutoTranscode  )
    Q_PROPERTY( bool            AutoMetaLookup  READ AutoMetaLookup   WRITE setAutoMetaLookup )
    Q_PROPERTY( bool            AutoUserJob1    READ AutoUserJob1     WRITE setAutoUserJob1   )
    Q_PROPERTY( bool            AutoUserJob2    READ AutoUserJob2     WRITE setAutoUserJob2   )
    Q_PROPERTY( bool            AutoUserJob3    READ AutoUserJob3     WRITE setAutoUserJob3   )
    Q_PROPERTY( bool            AutoUserJob4    READ AutoUserJob4     WRITE setAutoUserJob4   )
    Q_PROPERTY( int             Transcoder      READ Transcoder       WRITE setTranscoder     )

    Q_PROPERTY( QDateTime       NextRecording   READ NextRecording    WRITE setNextRecording  )
    Q_PROPERTY( QDateTime       LastRecorded    READ LastRecorded     WRITE setLastRecorded   )
    Q_PROPERTY( QDateTime       LastDeleted     READ LastDeleted      WRITE setLastDeleted    )
    Q_PROPERTY( int             AverageDelay    READ Transcoder       WRITE setTranscoder     )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP    ( int        , ParentId       )
    PROPERTYIMP    ( bool       , Inactive       )
    PROPERTYIMP    ( QString    , Title          )
    PROPERTYIMP    ( QString    , SubTitle       )
    PROPERTYIMP    ( QString    , Description    )
    PROPERTYIMP    ( uint       , Season         )
    PROPERTYIMP    ( uint       , Episode        )
    PROPERTYIMP    ( QString    , Category       )
    PROPERTYIMP    ( QDateTime  , StartTime      )
    PROPERTYIMP    ( QDateTime  , EndTime        )
    PROPERTYIMP    ( QString    , SeriesId       )
    PROPERTYIMP    ( QString    , ProgramId      )
    PROPERTYIMP    ( QString    , Inetref        )
    PROPERTYIMP    ( int        , ChanId         )
    PROPERTYIMP    ( QString    , CallSign       )
    PROPERTYIMP    ( int        , FindDay        )
    PROPERTYIMP    ( QTime      , FindTime       )
    PROPERTYIMP    ( QString    , Type           )
    PROPERTYIMP    ( QString    , SearchType     )
    PROPERTYIMP    ( int        , RecPriority    )
    PROPERTYIMP    ( uint       , PreferredInput )
    PROPERTYIMP    ( int        , StartOffset    )
    PROPERTYIMP    ( int        , EndOffset      )
    PROPERTYIMP    ( QString    , DupMethod      )
    PROPERTYIMP    ( QString    , DupIn          )
    PROPERTYIMP    ( uint       , Filter         )
    PROPERTYIMP    ( QString    , RecProfile     )
    PROPERTYIMP    ( QString    , RecGroup       )
    PROPERTYIMP    ( QString    , StorageGroup   )
    PROPERTYIMP    ( QString    , PlayGroup      )
    PROPERTYIMP    ( bool       , AutoExpire     )
    PROPERTYIMP    ( int        , MaxEpisodes    )
    PROPERTYIMP    ( bool       , MaxNewest      )
    PROPERTYIMP    ( bool       , AutoCommflag   )
    PROPERTYIMP    ( bool       , AutoTranscode  )
    PROPERTYIMP    ( bool       , AutoMetaLookup )
    PROPERTYIMP    ( bool       , AutoUserJob1   )
    PROPERTYIMP    ( bool       , AutoUserJob2   )
    PROPERTYIMP    ( bool       , AutoUserJob3   )
    PROPERTYIMP    ( bool       , AutoUserJob4   )
    PROPERTYIMP    ( int        , Transcoder     )
    PROPERTYIMP    ( QDateTime  , NextRecording  )
    PROPERTYIMP    ( QDateTime  , LastRecorded   )
    PROPERTYIMP    ( QDateTime  , LastDeleted    )
    PROPERTYIMP    ( int        , AverageDelay   )

    public:

        static inline void InitializeCustomTypes();

    public:

        RecRule(QObject *parent = 0)
            : QObject         ( parent ),
              m_Id            ( 0      ),
              m_ParentId      ( 0      ),
              m_Inactive      ( false  ),
              m_Season        ( 0      ),
              m_Episode       ( 0      ),
              m_ChanId        ( 0      ),
              m_FindDay       ( 0      ),
              m_RecPriority   ( 0      ),
              m_PreferredInput( 0      ),
              m_StartOffset   ( 0      ),
              m_EndOffset     ( 0      ),
              m_Filter        ( 0      ),
              m_AutoExpire    ( false  ),
              m_MaxEpisodes   ( 0      ),
              m_MaxNewest     ( false  ),
              m_AutoCommflag  ( false  ),
              m_AutoTranscode ( false  ),
              m_AutoMetaLookup( false  ),
              m_AutoUserJob1  ( false  ),
              m_AutoUserJob2  ( false  ),
              m_AutoUserJob3  ( false  ),
              m_AutoUserJob4  ( false  ),
              m_Transcoder    ( 0      ),
              m_AverageDelay  ( 0      )
        {
        }

        RecRule( const RecRule &src )
        {
            Copy( src );
        }

        void Copy( const RecRule &src )
        {
            m_Id            = src.m_Id            ;
            m_ParentId      = src.m_ParentId      ;
            m_Inactive      = src.m_Inactive      ;
            m_Title         = src.m_Title         ;
            m_SubTitle      = src.m_SubTitle      ;
            m_Description   = src.m_Description   ;
            m_Season        = src.m_Season        ;
            m_Episode       = src.m_Episode       ;
            m_Category      = src.m_Category      ;
            m_StartTime     = src.m_StartTime     ;
            m_EndTime       = src.m_EndTime       ;
            m_SeriesId      = src.m_SeriesId      ;
            m_ProgramId     = src.m_ProgramId     ;
            m_Inetref       = src.m_Inetref       ;
            m_ChanId        = src.m_ChanId        ;
            m_CallSign      = src.m_CallSign      ;
            m_FindDay       = src.m_FindDay       ;
            m_FindTime      = src.m_FindTime      ;
            m_Type          = src.m_Type          ;
            m_SearchType    = src.m_SearchType    ;
            m_RecPriority   = src.m_RecPriority   ;
            m_PreferredInput= src.m_PreferredInput;
            m_StartOffset   = src.m_StartOffset   ;
            m_EndOffset     = src.m_EndOffset     ;
            m_DupMethod     = src.m_DupMethod     ;
            m_DupIn         = src.m_DupIn         ;
            m_Filter        = src.m_Filter        ;
            m_RecProfile    = src.m_RecProfile    ;
            m_RecGroup      = src.m_RecGroup      ;
            m_StorageGroup  = src.m_StorageGroup  ;
            m_PlayGroup     = src.m_PlayGroup     ;
            m_AutoExpire    = src.m_AutoExpire    ;
            m_MaxEpisodes   = src.m_MaxEpisodes   ;
            m_MaxNewest     = src.m_MaxNewest     ;
            m_AutoCommflag  = src.m_AutoCommflag  ;
            m_AutoTranscode = src.m_AutoTranscode ;
            m_AutoMetaLookup= src.m_AutoMetaLookup;
            m_AutoUserJob1  = src.m_AutoUserJob1  ;
            m_AutoUserJob2  = src.m_AutoUserJob2  ;
            m_AutoUserJob3  = src.m_AutoUserJob3  ;
            m_AutoUserJob4  = src.m_AutoUserJob4  ;
            m_Transcoder    = src.m_Transcoder    ;
            m_NextRecording = src.m_NextRecording ;
            m_LastRecorded  = src.m_LastRecorded  ;
            m_LastDeleted   = src.m_LastDeleted   ;
            m_AverageDelay  = src.m_AverageDelay  ;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::RecRule  )
Q_DECLARE_METATYPE( DTC::RecRule* )

namespace DTC
{
inline void RecRule::InitializeCustomTypes()
{
    qRegisterMetaType< RecRule   >();
    qRegisterMetaType< RecRule*  >();
}
}

#endif

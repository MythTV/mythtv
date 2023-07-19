#ifndef V2RECRULE_H_
#define V2RECRULE_H_

#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class  V2RecRule : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "2.10" );

    SERVICE_PROPERTY2( int        , Id             )
    SERVICE_PROPERTY2( int        , ParentId       )
    SERVICE_PROPERTY2( bool       , Inactive       )
    SERVICE_PROPERTY2( QString    , Title          )
    SERVICE_PROPERTY2( QString    , SubTitle       )
    SERVICE_PROPERTY2( QString    , Description    )
    SERVICE_PROPERTY2( uint       , Season         )
    SERVICE_PROPERTY2( uint       , Episode        )
    SERVICE_PROPERTY2( QString    , Category       )
    SERVICE_PROPERTY2( QDateTime  , StartTime      )
    SERVICE_PROPERTY2( QDateTime  , EndTime        )
    SERVICE_PROPERTY2( QString    , SeriesId       )
    SERVICE_PROPERTY2( QString    , ProgramId      )
    SERVICE_PROPERTY2( QString    , Inetref        )
    SERVICE_PROPERTY2( int        , ChanId         )
    SERVICE_PROPERTY2( QString    , CallSign       )
    SERVICE_PROPERTY2( int        , FindDay        )
    SERVICE_PROPERTY2( QTime      , FindTime       )
    SERVICE_PROPERTY2( QString    , Type           )
    SERVICE_PROPERTY2( QString    , SearchType     )
    SERVICE_PROPERTY2( int        , RecPriority    )
    SERVICE_PROPERTY2( uint       , PreferredInput )
    SERVICE_PROPERTY2( int        , StartOffset    )
    SERVICE_PROPERTY2( int        , EndOffset      )
    SERVICE_PROPERTY2( QString    , DupMethod      )
    SERVICE_PROPERTY2( QString    , DupIn          )
    SERVICE_PROPERTY2( bool       , NewEpisOnly    )
    SERVICE_PROPERTY2( uint       , Filter         )
    SERVICE_PROPERTY2( QString    , RecProfile     )
    SERVICE_PROPERTY2( QString    , RecGroup       )
    SERVICE_PROPERTY2( QString    , StorageGroup   )
    SERVICE_PROPERTY2( QString    , PlayGroup      )
    SERVICE_PROPERTY2( bool       , AutoExpire     )
    SERVICE_PROPERTY2( int        , MaxEpisodes    )
    SERVICE_PROPERTY2( bool       , MaxNewest      )
    SERVICE_PROPERTY2( bool       , AutoCommflag   )
    SERVICE_PROPERTY2( bool       , AutoTranscode  )
    SERVICE_PROPERTY2( bool       , AutoMetaLookup )
    SERVICE_PROPERTY2( bool       , AutoUserJob1   )
    SERVICE_PROPERTY2( bool       , AutoUserJob2   )
    SERVICE_PROPERTY2( bool       , AutoUserJob3   )
    SERVICE_PROPERTY2( bool       , AutoUserJob4   )
    SERVICE_PROPERTY2( int        , Transcoder     )
    SERVICE_PROPERTY2( QDateTime  , NextRecording  )
    SERVICE_PROPERTY2( QDateTime  , LastRecorded   )
    SERVICE_PROPERTY2( QDateTime  , LastDeleted    )
    SERVICE_PROPERTY2( int        , AverageDelay   )
    SERVICE_PROPERTY2( QString    , AutoExtend   )

    public:

        Q_INVOKABLE V2RecRule(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2RecRule *src )
        {
            m_Id            = src->m_Id            ;
            m_ParentId      = src->m_ParentId      ;
            m_Inactive      = src->m_Inactive      ;
            m_Title         = src->m_Title         ;
            m_SubTitle      = src->m_SubTitle      ;
            m_Description   = src->m_Description   ;
            m_Season        = src->m_Season        ;
            m_Episode       = src->m_Episode       ;
            m_Category      = src->m_Category      ;
            m_StartTime     = src->m_StartTime     ;
            m_EndTime       = src->m_EndTime       ;
            m_SeriesId      = src->m_SeriesId      ;
            m_ProgramId     = src->m_ProgramId     ;
            m_Inetref       = src->m_Inetref       ;
            m_ChanId        = src->m_ChanId        ;
            m_CallSign      = src->m_CallSign      ;
            m_FindDay       = src->m_FindDay       ;
            m_FindTime      = src->m_FindTime      ;
            m_Type          = src->m_Type          ;
            m_SearchType    = src->m_SearchType    ;
            m_RecPriority   = src->m_RecPriority   ;
            m_PreferredInput= src->m_PreferredInput;
            m_StartOffset   = src->m_StartOffset   ;
            m_EndOffset     = src->m_EndOffset     ;
            m_DupMethod     = src->m_DupMethod     ;
            m_DupIn         = src->m_DupIn         ;
            m_NewEpisOnly   = src->m_NewEpisOnly   ;
            m_Filter        = src->m_Filter        ;
            m_RecProfile    = src->m_RecProfile    ;
            m_RecGroup      = src->m_RecGroup      ;
            m_StorageGroup  = src->m_StorageGroup  ;
            m_PlayGroup     = src->m_PlayGroup     ;
            m_AutoExpire    = src->m_AutoExpire    ;
            m_MaxEpisodes   = src->m_MaxEpisodes   ;
            m_MaxNewest     = src->m_MaxNewest     ;
            m_AutoCommflag  = src->m_AutoCommflag  ;
            m_AutoTranscode = src->m_AutoTranscode ;
            m_AutoMetaLookup= src->m_AutoMetaLookup;
            m_AutoUserJob1  = src->m_AutoUserJob1  ;
            m_AutoUserJob2  = src->m_AutoUserJob2  ;
            m_AutoUserJob3  = src->m_AutoUserJob3  ;
            m_AutoUserJob4  = src->m_AutoUserJob4  ;
            m_Transcoder    = src->m_Transcoder    ;
            m_NextRecording = src->m_NextRecording ;
            m_LastRecorded  = src->m_LastRecorded  ;
            m_LastDeleted   = src->m_LastDeleted   ;
            m_AverageDelay  = src->m_AverageDelay  ;
            m_AutoExtend    = src->m_AutoExtend    ;
        }

    private:
        Q_DISABLE_COPY(V2RecRule);
};

Q_DECLARE_METATYPE(V2RecRule*)

#endif

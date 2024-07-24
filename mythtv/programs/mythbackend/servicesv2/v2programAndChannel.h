//////////////////////////////////////////////////////////////////////////////
// Program Name: program.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2PROGRAMANDCHANNEL_H_
#define V2PROGRAMANDCHANNEL_H_

#include <QDateTime>
#include <QString>

#include "libmythbase/http/mythhttpservice.h"

#include "v2recording.h"
#include "v2artworkInfoList.h"
#include "v2castMemberList.h"


class V2Program;


class V2ChannelInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "2.4" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Programs", "type=V2Program");

    SERVICE_PROPERTY2( uint        , ChanId         )
    SERVICE_PROPERTY2( QString     , ChanNum        )
    SERVICE_PROPERTY2( QString     , CallSign       )
    SERVICE_PROPERTY2( QString     , IconURL        )
    SERVICE_PROPERTY2( QString     , Icon           )
    SERVICE_PROPERTY2( QString     , ChannelName    )
    SERVICE_PROPERTY2( uint        , MplexId        )
    SERVICE_PROPERTY2( uint        , ServiceId      )
    SERVICE_PROPERTY2( uint        , ATSCMajorChan  )
    SERVICE_PROPERTY2( uint        , ATSCMinorChan  )
    SERVICE_PROPERTY2( QString     , Format         )
    SERVICE_PROPERTY2( QString     , FrequencyId    )
    SERVICE_PROPERTY2( int         , FineTune       )
    SERVICE_PROPERTY2( QString     , ChanFilters    )
    SERVICE_PROPERTY2( int         , SourceId       )
    SERVICE_PROPERTY2( int         , InputId        )
    SERVICE_PROPERTY2( bool        , CommFree       )
    SERVICE_PROPERTY2( bool        , UseEIT         )
    SERVICE_PROPERTY2( bool        , Visible        )
    SERVICE_PROPERTY2( QString     , ExtendedVisible )
    SERVICE_PROPERTY2( QString     , XMLTVID        )
    SERVICE_PROPERTY2( QString     , DefaultAuth    )
    SERVICE_PROPERTY2( QString     , ChannelGroups  )
    SERVICE_PROPERTY2( QString     , Inputs         )
    SERVICE_PROPERTY2( uint        , ServiceType    )
    SERVICE_PROPERTY2( int         , RecPriority    )
    SERVICE_PROPERTY2( int         , TimeOffset    )
    SERVICE_PROPERTY2( int         , CommMethod    )
    SERVICE_PROPERTY2( QVariantList, Programs       )

    public:

        Q_INVOKABLE V2ChannelInfo(QObject *parent = nullptr)
            :   QObject           ( parent ),
                m_Visible           (true)
        {
        }

        void Copy( const V2ChannelInfo *src )
        {
            m_ChanId        = src->m_ChanId      ;
            m_ChanNum       = src->m_ChanNum     ;
            m_CallSign      = src->m_CallSign    ;
            m_IconURL       = src->m_IconURL     ;
            m_Icon          = src->m_Icon        ;
            m_ChannelName   = src->m_ChannelName ;
            m_ChanFilters   = src->m_ChanFilters ;
            m_SourceId      = src->m_SourceId    ;
            m_InputId       = src->m_InputId     ;
            m_CommFree      = src->m_CommFree    ;
            m_UseEIT        = src->m_UseEIT      ;
            m_Visible       = src->m_Visible     ;
            m_ExtendedVisible = src->m_ExtendedVisible;
            m_ChannelGroups = src->m_ChannelGroups;
            m_Inputs        = src->m_Inputs;
            m_RecPriority   = src->m_RecPriority;
            m_TimeOffset    = src->m_TimeOffset;
            m_CommMethod    = src->m_CommMethod;

            CopyListContents< V2Program >( this, m_Programs, src->m_Programs );
        }

        V2Program *AddNewProgram();

    private:
        Q_DISABLE_COPY(V2ChannelInfo);
};

Q_DECLARE_METATYPE(V2ChannelInfo*)

class V2Program : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.12" );
    Q_CLASSINFO( "defaultProp", "Description" );
    Q_CLASSINFO( "deprecated" , "FileSize,FileName,HostName");


    SERVICE_PROPERTY2( QDateTime   , StartTime    )
    SERVICE_PROPERTY2( QDateTime   , EndTime      )
    SERVICE_PROPERTY2( QString     , Title        )
    SERVICE_PROPERTY2( QString     , SubTitle     )
    SERVICE_PROPERTY2( QString     , Category     )
    SERVICE_PROPERTY2( QString     , CatType      )
    SERVICE_PROPERTY2( bool        , Repeat       )
    SERVICE_PROPERTY2( QString     , SeriesId     )
    SERVICE_PROPERTY2( QString     , ProgramId    )
    SERVICE_PROPERTY2( double      , Stars        )
    SERVICE_PROPERTY2( QDateTime   , LastModified )
    SERVICE_PROPERTY2( int         , ProgramFlags )
    SERVICE_PROPERTY2( QString     , ProgramFlagNames )
    SERVICE_PROPERTY2( int         , VideoProps   )
    SERVICE_PROPERTY2( QString     , VideoPropNames )
    SERVICE_PROPERTY2( int         , AudioProps   )
    SERVICE_PROPERTY2( QString     , AudioPropNames )
    SERVICE_PROPERTY2( int         , SubProps     )
    SERVICE_PROPERTY2( QString     , SubPropNames )
    SERVICE_PROPERTY2( QDate       , Airdate      )
    SERVICE_PROPERTY2( QString     , Description  )
    SERVICE_PROPERTY2( QString     , Inetref      )
    SERVICE_PROPERTY2( int         , Season       )
    SERVICE_PROPERTY2( int         , Episode      )
    SERVICE_PROPERTY2( int         , TotalEpisodes)

    // ----
    // DEPRECATED
    // These don't belong here, they are Recording only metadata
    SERVICE_PROPERTY2( qlonglong   , FileSize     )
    SERVICE_PROPERTY2( QString     , FileName     )
    SERVICE_PROPERTY2( QString     , HostName     )
    // ----

    Q_PROPERTY( QObject*    Channel      READ Channel     USER true)
    Q_PROPERTY( QObject*    Recording    READ Recording   USER true)
    Q_PROPERTY( QObject*    Artwork      READ Artwork     USER true)
    Q_PROPERTY( QObject*    Cast         READ Cast        USER true)


    SERVICE_PROPERTY_COND_PTR( V2ChannelInfo    , Channel     )
    SERVICE_PROPERTY_COND_PTR( V2RecordingInfo  , Recording   )
    SERVICE_PROPERTY_COND_PTR( V2ArtworkInfoList, Artwork     )
    SERVICE_PROPERTY_COND_PTR( V2CastMemberList , Cast        )

    public:

        Q_INVOKABLE V2Program(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2Program *src )
        {
            m_StartTime         = src->m_StartTime;
            m_EndTime           = src->m_EndTime;
            m_Title             = src->m_Title;
            m_SubTitle          = src->m_SubTitle;
            m_Category          = src->m_Category;
            m_CatType           = src->m_CatType;
            m_Repeat            = src->m_Repeat;
            m_SeriesId          = src->m_SeriesId;
            m_ProgramId         = src->m_ProgramId;
            m_Stars             = src->m_Stars;
            m_LastModified      = src->m_LastModified;
            m_ProgramFlags      = src->m_ProgramFlags;
            m_VideoProps        = src->m_VideoProps;
            m_AudioProps        = src->m_AudioProps;
            m_SubProps          = src->m_SubProps;
            m_Airdate           = src->m_Airdate;
            m_Description       = src->m_Description;
            m_Inetref           = src->m_Inetref;
            m_Season            = src->m_Season;
            m_Episode           = src->m_Episode;
            m_TotalEpisodes     = src->m_TotalEpisodes;
            // DEPRECATED
            m_FileSize          = src->m_FileSize;
            m_FileName          = src->m_FileName;
            m_HostName          = src->m_HostName;

            if ( src->m_Channel != nullptr)
                Channel()->Copy( src->m_Channel );

            if ( src->m_Recording != nullptr)
                Recording()->Copy( src->m_Recording );

            if ( src->m_Artwork != nullptr)
                Artwork()->Copy( src->m_Artwork );

            if (src->m_Cast != nullptr)
                Cast()->Copy( src->m_Cast );
        }

    private:
        Q_DISABLE_COPY(V2Program);
};

inline V2Program *V2ChannelInfo::AddNewProgram()
{
    // We must make sure the object added to the QVariantList has
    // a parent of 'this'

    auto *pObject = new V2Program( this );
    m_Programs.append( QVariant::fromValue<QObject *>( pObject ));

    return pObject;
}

Q_DECLARE_METATYPE(V2Program*)

#endif

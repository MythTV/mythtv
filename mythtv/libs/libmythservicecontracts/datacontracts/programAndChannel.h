//////////////////////////////////////////////////////////////////////////////
// Program Name: program.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <QDateTime>
#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "recording.h"
#include "artworkInfoList.h"
#include "castMemberList.h"

namespace DTC
{

class Program;


class SERVICE_PUBLIC ChannelInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "2.2" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Programs", "type=DTC::Program");

    Q_PROPERTY( uint      ChanId          READ ChanId          WRITE setChanId          )
    Q_PROPERTY( QString   ChanNum         READ ChanNum         WRITE setChanNum         )
    Q_PROPERTY( QString   CallSign        READ CallSign        WRITE setCallSign        )
    Q_PROPERTY( QString   IconURL         READ IconURL         WRITE setIconURL         )
    Q_PROPERTY( QString   ChannelName     READ ChannelName     WRITE setChannelName     )

    Q_PROPERTY( uint      MplexId         READ MplexId         WRITE setMplexId         )
    Q_PROPERTY( uint      ServiceId       READ ServiceId       WRITE setServiceId       )
    Q_PROPERTY( uint      ATSCMajorChan   READ ATSCMajorChan   WRITE setATSCMajorChan   )
    Q_PROPERTY( uint      ATSCMinorChan   READ ATSCMinorChan   WRITE setATSCMinorChan   )
    Q_PROPERTY( QString   Format          READ Format          WRITE setFormat          )
    Q_PROPERTY( QString   FrequencyId     READ FrequencyId     WRITE setFrequencyId     )
    Q_PROPERTY( int       FineTune        READ FineTune        WRITE setFineTune        )
    Q_PROPERTY( QString   ChanFilters     READ ChanFilters     WRITE setChanFilters     )
    Q_PROPERTY( int       SourceId        READ SourceId        WRITE setSourceId        )
    Q_PROPERTY( int       InputId         READ InputId         WRITE setInputId         )
    Q_PROPERTY( bool      CommFree        READ CommFree        WRITE setCommFree        )
    Q_PROPERTY( bool      UseEIT          READ UseEIT          WRITE setUseEIT          )
    Q_PROPERTY( bool      Visible         READ Visible         WRITE setVisible         )
    Q_PROPERTY( QString   ExtendedVisible READ ExtendedVisible WRITE setExtendedVisible )
    Q_PROPERTY( QString   XMLTVID         READ XMLTVID         WRITE setXMLTVID         )
    Q_PROPERTY( QString   DefaultAuth     READ DefaultAuth     WRITE setDefaultAuth     )
    Q_PROPERTY( QString   ChannelGroups   READ ChannelGroups   WRITE setChannelGroups   )
    Q_PROPERTY( QString   Inputs          READ Inputs          WRITE setInputs          )
    Q_PROPERTY( uint      ServiceType     READ ServiceType     WRITE setServiceType     )

    Q_PROPERTY( QVariantList Programs    READ Programs )

    PROPERTYIMP       ( uint        , ChanId         )
    PROPERTYIMP_REF   ( QString     , ChanNum        )
    PROPERTYIMP_REF   ( QString     , CallSign       )
    PROPERTYIMP_REF   ( QString     , IconURL        )
    PROPERTYIMP_REF   ( QString     , ChannelName    )
    PROPERTYIMP       ( uint        , MplexId        )
    PROPERTYIMP       ( uint        , ServiceId      )
    PROPERTYIMP       ( uint        , ATSCMajorChan  )
    PROPERTYIMP       ( uint        , ATSCMinorChan  )
    PROPERTYIMP_REF   ( QString     , Format         )
    PROPERTYIMP_REF   ( QString     , FrequencyId    )
    PROPERTYIMP       ( int         , FineTune       )
    PROPERTYIMP_REF   ( QString     , ChanFilters    )
    PROPERTYIMP       ( int         , SourceId       )
    PROPERTYIMP       ( int         , InputId        )
    PROPERTYIMP       ( bool        , CommFree       )
    PROPERTYIMP       ( bool        , UseEIT         )
    PROPERTYIMP       ( bool        , Visible        )
    PROPERTYIMP_REF   ( QString     , ExtendedVisible )
    PROPERTYIMP_REF   ( QString     , XMLTVID        )
    PROPERTYIMP_REF   ( QString     , DefaultAuth    )
    PROPERTYIMP_REF   ( QString     , ChannelGroups  )
    PROPERTYIMP_REF   ( QString     , Inputs         )
    PROPERTYIMP       ( uint        , ServiceType    )

    PROPERTYIMP_RO_REF( QVariantList, Programs       )

    // Used only by Serializer
    PROPERTYIMP( bool, SerializeDetails )

    public:

        static void InitializeCustomTypes();

        Q_INVOKABLE explicit ChannelInfo(QObject *parent = nullptr)
            : QObject           ( parent ),
              m_ChanId          ( 0      ),
              m_MplexId         ( 0      ),
              m_ServiceId       ( 0      ),
              m_ATSCMajorChan   ( 0      ),
              m_ATSCMinorChan   ( 0      ),
              m_FineTune        ( 0      ),
              m_SourceId        ( 0      ),
              m_InputId         ( 0      ),
              m_CommFree        ( false  ),
              m_UseEIT          ( false  ),
              m_Visible         ( true   ),
              m_ServiceType     ( 0      ),
              m_SerializeDetails( true   )
        {
        }

        void Copy( const ChannelInfo *src )
        {
            m_ChanId        = src->m_ChanId      ;
            m_ChanNum       = src->m_ChanNum     ;
            m_CallSign      = src->m_CallSign    ;
            m_IconURL       = src->m_IconURL     ;
            m_ChannelName   = src->m_ChannelName ;
            m_ChanFilters   = src->m_ChanFilters ;
            m_SourceId      = src->m_SourceId    ;
            m_InputId       = src->m_InputId     ;
            m_CommFree      = src->m_CommFree    ;
            m_UseEIT        = src->m_UseEIT      ;
            m_Visible       = src->m_Visible     ;
            m_ChannelGroups = src->m_ChannelGroups;
            m_Inputs        = src->m_Inputs;

            CopyListContents< Program >( this, m_Programs, src->m_Programs );
        }

        Program *AddNewProgram();

    private:
        Q_DISABLE_COPY(ChannelInfo);
};

class SERVICE_PUBLIC Program : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.12" );
    Q_CLASSINFO( "defaultProp", "Description" );
    Q_CLASSINFO( "deprecated" , "FileSize,FileName,HostName");

    Q_PROPERTY( QDateTime   StartTime    READ StartTime    WRITE setStartTime )
    Q_PROPERTY( QDateTime   EndTime      READ EndTime      WRITE setEndTime   )
    Q_PROPERTY( QString     Title        READ Title        WRITE setTitle     )
    Q_PROPERTY( QString     SubTitle     READ SubTitle     WRITE setSubTitle  )
    Q_PROPERTY( QString     Category     READ Category     WRITE setCategory  )
    Q_PROPERTY( QString     CatType      READ CatType      WRITE setCatType   )
    Q_PROPERTY( bool        Repeat       READ Repeat       WRITE setRepeat    )
    Q_PROPERTY( int         VideoProps   READ VideoProps   WRITE setVideoProps)
    Q_PROPERTY( QString     VideoPropNames READ VideoPropNames WRITE setVideoPropNames  )
    Q_PROPERTY( int         AudioProps   READ AudioProps   WRITE setAudioProps)
    Q_PROPERTY( QString     AudioPropNames READ AudioPropNames WRITE setAudioPropNames  )
    Q_PROPERTY( int         SubProps     READ SubProps     WRITE setSubProps  )
    Q_PROPERTY( QString     SubPropNames READ SubPropNames WRITE setSubPropNames  )

    Q_PROPERTY( QString     SeriesId      READ SeriesId      WRITE setSeriesId      )
    Q_PROPERTY( QString     ProgramId     READ ProgramId     WRITE setProgramId     )
    Q_PROPERTY( double      Stars         READ Stars         WRITE setStars         )
    Q_PROPERTY( QDateTime   LastModified  READ LastModified  WRITE setLastModified  )
    Q_PROPERTY( int         ProgramFlags  READ ProgramFlags  WRITE setProgramFlags  )
    Q_PROPERTY( QString     ProgramFlagNames READ ProgramFlagNames WRITE setProgramFlagNames  )
    Q_PROPERTY( QDate       Airdate       READ Airdate       WRITE setAirdate       )
    Q_PROPERTY( QString     Description   READ Description   WRITE setDescription   )
    Q_PROPERTY( QString     Inetref       READ Inetref       WRITE setInetref       )
    Q_PROPERTY( int         Season        READ Season        WRITE setSeason        )
    Q_PROPERTY( int         Episode       READ Episode       WRITE setEpisode       )
    Q_PROPERTY( int         TotalEpisodes READ TotalEpisodes WRITE setTotalEpisodes )

    // ----
    // DEPRECATED
    // These don't belong here, they are Recording only metadata
    Q_PROPERTY( qlonglong   FileSize      READ FileSize      WRITE setFileSize      )
    Q_PROPERTY( QString     FileName      READ FileName      WRITE setFileName      )
    Q_PROPERTY( QString     HostName      READ HostName      WRITE setHostName      )
    // ----

    Q_PROPERTY( QObject*    Channel      READ Channel   )
    Q_PROPERTY( QObject*    Recording    READ Recording )
    Q_PROPERTY( QObject*    Artwork      READ Artwork   )
    Q_PROPERTY( QObject*    Cast         READ Cast      )

    PROPERTYIMP_REF( QDateTime   , StartTime    )
    PROPERTYIMP_REF( QDateTime   , EndTime      )
    PROPERTYIMP_REF( QString     , Title        )
    PROPERTYIMP_REF( QString     , SubTitle     )
    PROPERTYIMP_REF( QString     , Category     )
    PROPERTYIMP_REF( QString     , CatType      )
    PROPERTYIMP    ( bool        , Repeat       )

    PROPERTYIMP_REF( QString     , SeriesId     )
    PROPERTYIMP_REF( QString     , ProgramId    )
    PROPERTYIMP    ( double      , Stars        )
    PROPERTYIMP_REF( QDateTime   , LastModified )
    PROPERTYIMP    ( int         , ProgramFlags )
    PROPERTYIMP_REF( QString     , ProgramFlagNames )
    PROPERTYIMP    ( int         , VideoProps   )
    PROPERTYIMP_REF( QString     , VideoPropNames )
    PROPERTYIMP    ( int         , AudioProps   )
    PROPERTYIMP_REF( QString     , AudioPropNames )
    PROPERTYIMP    ( int         , SubProps     )
    PROPERTYIMP_REF( QString     , SubPropNames )
    PROPERTYIMP    ( QDate       , Airdate      )
    PROPERTYIMP_REF( QString     , Description  )
    PROPERTYIMP_REF( QString     , Inetref      )
    PROPERTYIMP    ( int         , Season       )
    PROPERTYIMP    ( int         , Episode      )
    PROPERTYIMP    ( int         , TotalEpisodes)

    // ----
    // DEPRECATED
    // These don't belong here, they are Recording only metadata
    PROPERTYIMP    ( qlonglong   , FileSize     )
    PROPERTYIMP_REF( QString     , FileName     )
    PROPERTYIMP_REF( QString     , HostName     )
    // ----

    PROPERTYIMP_PTR( ChannelInfo    , Channel     )
    PROPERTYIMP_PTR( RecordingInfo  , Recording   )
    PROPERTYIMP_PTR( ArtworkInfoList, Artwork     )
    PROPERTYIMP_PTR( CastMemberList , Cast        )

    // Used only by Serializer
    PROPERTYIMP( bool, SerializeDetails )
    PROPERTYIMP( bool, SerializeChannel )
    PROPERTYIMP( bool, SerializeRecording )
    PROPERTYIMP( bool, SerializeArtwork )
    PROPERTYIMP( bool, SerializeCast );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE explicit Program(QObject *parent = nullptr)
            : QObject               ( parent ),
              m_Repeat              ( false  ),
              m_Stars               ( 0      ),
              m_ProgramFlags        ( 0      ),
              m_VideoProps          ( 0      ),
              m_AudioProps          ( 0      ),
              m_SubProps            ( 0      ),
              m_Season              ( 0      ),
              m_Episode             ( 0      ),
              m_TotalEpisodes       ( 0      ),
              m_FileSize            ( 0      ), // DEPRECATED
              m_Channel             ( nullptr ),
              m_Recording           ( nullptr ),
              m_Artwork             ( nullptr ),
              m_Cast                ( nullptr ),
              m_SerializeDetails    ( true   ),
              m_SerializeChannel    ( true   ),
              m_SerializeRecording  ( true   ),
              m_SerializeArtwork    ( true   ),
              m_SerializeCast       ( true   )
        {
        }

        void Copy( const Program *src )
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
            // ----
            m_SerializeDetails  = src->m_SerializeDetails;
            m_SerializeChannel  = src->m_SerializeChannel;
            m_SerializeRecording= src->m_SerializeRecording;
            m_SerializeArtwork  = src->m_SerializeArtwork;
            m_SerializeCast     = src->m_SerializeCast;

            if ( src->m_Channel != nullptr)
                Channel()->Copy( src->m_Channel );

            if ( src->m_Recording != nullptr)
                Recording()->Copy( src->m_Recording );

            if ( src->m_Artwork != nullptr)
                Artwork()->Copy( src->m_Artwork );

            if (src->m_Cast != nullptr)
                Cast()->Copy( src->m_Cast );
        }

};

inline Program *ChannelInfo::AddNewProgram()
{
    // We must make sure the object added to the QVariantList has
    // a parent of 'this'

    auto *pObject = new Program( this );
    m_Programs.append( QVariant::fromValue<QObject *>( pObject ));

    return pObject;
}

inline void ChannelInfo::InitializeCustomTypes()
{
    qRegisterMetaType< ChannelInfo* >();

    if (qMetaTypeId<DTC::Program*>() == 0)
        Program::InitializeCustomTypes();
}

inline void Program::InitializeCustomTypes()
{
    qRegisterMetaType< Program* >();

    if (qMetaTypeId<DTC::ChannelInfo*>() == QMetaType::UnknownType)
        ChannelInfo::InitializeCustomTypes();

    if (qMetaTypeId<DTC::RecordingInfo*>() == QMetaType::UnknownType)
        RecordingInfo::InitializeCustomTypes();

    if (qMetaTypeId<DTC::ArtworkInfoList*>() == QMetaType::UnknownType)
        ArtworkInfoList::InitializeCustomTypes();

    if (qMetaTypeId<DTC::CastMemberList*>() == QMetaType::UnknownType)
        CastMemberList::InitializeCustomTypes();
}

} // namespace DTC

#endif

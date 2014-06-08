//////////////////////////////////////////////////////////////////////////////
// Program Name: program.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
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
    Q_CLASSINFO( "version", "2.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Programs", "type=DTC::Program");

    Q_PROPERTY( uint      ChanId          READ ChanId         WRITE setChanId        )
    Q_PROPERTY( QString   ChanNum         READ ChanNum        WRITE setChanNum       )
    Q_PROPERTY( QString   CallSign        READ CallSign       WRITE setCallSign      )
    Q_PROPERTY( QString   IconURL         READ IconURL        WRITE setIconURL       )
    Q_PROPERTY( QString   ChannelName     READ ChannelName    WRITE setChannelName   )

    Q_PROPERTY( uint      MplexId         READ MplexId        WRITE setMplexId        DESIGNABLE SerializeDetails )
    Q_PROPERTY( uint      ServiceId       READ ServiceId      WRITE setServiceId      DESIGNABLE SerializeDetails )
    Q_PROPERTY( uint      ATSCMajorChan   READ ATSCMajorChan  WRITE setATSCMajorChan  DESIGNABLE SerializeDetails )
    Q_PROPERTY( uint      ATSCMinorChan   READ ATSCMinorChan  WRITE setATSCMinorChan  DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   Format          READ Format         WRITE setFormat         DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   FrequencyId     READ FrequencyId    WRITE setFrequencyId    DESIGNABLE SerializeDetails )
    Q_PROPERTY( int       FineTune        READ FineTune       WRITE setFineTune       DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   ChanFilters     READ ChanFilters    WRITE setChanFilters    DESIGNABLE SerializeDetails )
    Q_PROPERTY( int       SourceId        READ SourceId       WRITE setSourceId       DESIGNABLE SerializeDetails )
    Q_PROPERTY( int       InputId         READ InputId        WRITE setInputId        DESIGNABLE SerializeDetails )
    Q_PROPERTY( bool      CommFree        READ CommFree       WRITE setCommFree       DESIGNABLE SerializeDetails )
    Q_PROPERTY( bool      UseEIT          READ UseEIT         WRITE setUseEIT         DESIGNABLE SerializeDetails )
    Q_PROPERTY( bool      Visible         READ Visible        WRITE setVisible        DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   XMLTVID         READ XMLTVID        WRITE setXMLTVID        DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   DefaultAuth     READ DefaultAuth    WRITE setDefaultAuth    DESIGNABLE SerializeDetails )

    Q_PROPERTY( QVariantList Programs    READ Programs DESIGNABLE true )

    PROPERTYIMP       ( uint        , ChanId         )
    PROPERTYIMP       ( QString     , ChanNum        )
    PROPERTYIMP       ( QString     , CallSign       )
    PROPERTYIMP       ( QString     , IconURL        )
    PROPERTYIMP       ( QString     , ChannelName    )
    PROPERTYIMP       ( uint        , MplexId        )
    PROPERTYIMP       ( uint        , ServiceId      )
    PROPERTYIMP       ( uint        , ATSCMajorChan  )
    PROPERTYIMP       ( uint        , ATSCMinorChan  )
    PROPERTYIMP       ( QString     , Format         )
    PROPERTYIMP       ( QString     , FrequencyId    )
    PROPERTYIMP       ( int         , FineTune       )
    PROPERTYIMP       ( QString     , ChanFilters    )
    PROPERTYIMP       ( int         , SourceId       )
    PROPERTYIMP       ( int         , InputId        )
    PROPERTYIMP       ( bool        , CommFree       )
    PROPERTYIMP       ( bool        , UseEIT         )
    PROPERTYIMP       ( bool        , Visible        )
    PROPERTYIMP       ( QString     , XMLTVID        )
    PROPERTYIMP       ( QString     , DefaultAuth    )

    PROPERTYIMP_RO_REF( QVariantList, Programs      )

    // Used only by Serializer
    PROPERTYIMP( bool, SerializeDetails )

    public:

        static void InitializeCustomTypes();

    public:

        ChannelInfo(QObject *parent = 0) 
            : QObject           ( parent ),
              m_ChanId          ( 0      ),
              m_MplexId         ( 0      ),
              m_ServiceId       ( 0      ),
              m_ATSCMajorChan   ( 0      ),
              m_ATSCMinorChan   ( 0      ),
              m_FineTune        ( 0      ),
              m_SourceId        ( 0      ),
              m_InputId         ( 0      ),
              m_CommFree        ( 0      ),
              m_UseEIT          ( false  ),
              m_Visible         ( true   ),
              m_SerializeDetails( true   )
        {
        }
        
        ChannelInfo( const ChannelInfo &src )
        {
            Copy( src );
        }

        void Copy( const ChannelInfo &src )
        {
            m_ChanId       = src.m_ChanId      ;
            m_ChanNum      = src.m_ChanNum     ;
            m_CallSign     = src.m_CallSign    ;
            m_IconURL      = src.m_IconURL     ;
            m_ChannelName  = src.m_ChannelName ;
            m_ChanFilters  = src.m_ChanFilters ;
            m_SourceId     = src.m_SourceId    ;
            m_InputId      = src.m_InputId     ;
            m_CommFree     = src.m_CommFree    ;
            m_UseEIT       = src.m_UseEIT      ;
            m_Visible      = src.m_Visible     ;

            CopyListContents< Program >( this, m_Programs, src.m_Programs );
        }

        Program *AddNewProgram();

};

class SERVICE_PUBLIC Program : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.11" );
    Q_CLASSINFO( "defaultProp", "Description" );

    Q_PROPERTY( QDateTime   StartTime    READ StartTime    WRITE setStartTime )
    Q_PROPERTY( QDateTime   EndTime      READ EndTime      WRITE setEndTime   )
    Q_PROPERTY( QString     Title        READ Title        WRITE setTitle     )
    Q_PROPERTY( QString     SubTitle     READ SubTitle     WRITE setSubTitle  )
    Q_PROPERTY( QString     Category     READ Category     WRITE setCategory  )
    Q_PROPERTY( QString     CatType      READ CatType      WRITE setCatType   )
    Q_PROPERTY( bool        Repeat       READ Repeat       WRITE setRepeat    )
    Q_PROPERTY( int         VideoProps   READ VideoProps   WRITE setVideoProps)
    Q_PROPERTY( int         AudioProps   READ AudioProps   WRITE setAudioProps)
    Q_PROPERTY( int         SubProps     READ SubProps     WRITE setSubProps  )

    Q_PROPERTY( QString     SeriesId      READ SeriesId      WRITE setSeriesId      DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString     ProgramId     READ ProgramId     WRITE setProgramId     DESIGNABLE SerializeDetails )
    Q_PROPERTY( double      Stars         READ Stars         WRITE setStars         DESIGNABLE SerializeDetails )
    Q_PROPERTY( qlonglong   FileSize      READ FileSize      WRITE setFileSize      DESIGNABLE SerializeDetails )
    Q_PROPERTY( QDateTime   LastModified  READ LastModified  WRITE setLastModified  DESIGNABLE SerializeDetails )
    Q_PROPERTY( int         ProgramFlags  READ ProgramFlags  WRITE setProgramFlags  DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString     FileName      READ FileName      WRITE setFileName      DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString     HostName      READ HostName      WRITE setHostName      DESIGNABLE SerializeDetails )
    Q_PROPERTY( QDate       Airdate       READ Airdate       WRITE setAirdate       DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString     Description   READ Description   WRITE setDescription   DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString     Inetref       READ Inetref       WRITE setInetref       DESIGNABLE SerializeDetails )
    Q_PROPERTY( int         Season        READ Season        WRITE setSeason        DESIGNABLE SerializeDetails )
    Q_PROPERTY( int         Episode       READ Episode       WRITE setEpisode       DESIGNABLE SerializeDetails )
    Q_PROPERTY( int         TotalEpisodes READ TotalEpisodes WRITE setTotalEpisodes DESIGNABLE SerializeDetails )

    Q_PROPERTY( QObject*    Channel      READ Channel   DESIGNABLE SerializeChannel )
    Q_PROPERTY( QObject*    Recording    READ Recording DESIGNABLE SerializeRecording )
    Q_PROPERTY( QObject*    Artwork      READ Artwork   DESIGNABLE SerializeArtwork )
    Q_PROPERTY( QObject*    Cast         READ Cast      DESIGNABLE SerializeCast )

    PROPERTYIMP    ( QDateTime   , StartTime    )
    PROPERTYIMP    ( QDateTime   , EndTime      )
    PROPERTYIMP    ( QString     , Title        )
    PROPERTYIMP    ( QString     , SubTitle     )
    PROPERTYIMP    ( QString     , Category     )
    PROPERTYIMP    ( QString     , CatType      )
    PROPERTYIMP    ( bool        , Repeat       )

    PROPERTYIMP    ( QString     , SeriesId     )
    PROPERTYIMP    ( QString     , ProgramId    )
    PROPERTYIMP    ( double      , Stars        )
    PROPERTYIMP    ( qlonglong   , FileSize     )
    PROPERTYIMP    ( QDateTime   , LastModified )
    PROPERTYIMP    ( int         , ProgramFlags )
    PROPERTYIMP    ( int         , VideoProps   )
    PROPERTYIMP    ( int         , AudioProps   )
    PROPERTYIMP    ( int         , SubProps     )
    PROPERTYIMP    ( QString     , FileName     )
    PROPERTYIMP    ( QString     , HostName     )
    PROPERTYIMP    ( QDate       , Airdate      )
    PROPERTYIMP    ( QString     , Description  )
    PROPERTYIMP    ( QString     , Inetref      )
    PROPERTYIMP    ( int         , Season       )
    PROPERTYIMP    ( int         , Episode      )
    PROPERTYIMP    ( int         , TotalEpisodes)

    PROPERTYIMP_PTR( ChannelInfo    , Channel     )
    PROPERTYIMP_PTR( RecordingInfo  , Recording   )
    PROPERTYIMP_PTR( ArtworkInfoList, Artwork     )
    PROPERTYIMP_PTR( CastMemberList , Cast        )

    // Used only by Serializer
    PROPERTYIMP( bool, SerializeDetails )
    PROPERTYIMP( bool, SerializeChannel )
    PROPERTYIMP( bool, SerializeRecording )
    PROPERTYIMP( bool, SerializeArtwork )
    PROPERTYIMP( bool, SerializeCast )

    public:

        static inline void InitializeCustomTypes();

    public:

        Program(QObject *parent = 0) 
            : QObject               ( parent ),
              m_Repeat              ( false  ),
              m_Stars               ( 0      ),
              m_FileSize            ( 0      ),
              m_ProgramFlags        ( 0      ),
              m_VideoProps          ( 0      ),
              m_AudioProps          ( 0      ),
              m_SubProps            ( 0      ),
              m_Season              ( 0      ),
              m_Episode             ( 0      ),
              m_TotalEpisodes       ( 0      ),
              m_Channel             ( NULL   ),
              m_Recording           ( NULL   ),
              m_Artwork             ( NULL   ),
              m_Cast                ( NULL   ),
              m_SerializeDetails    ( true   ),
              m_SerializeChannel    ( true   ),
              m_SerializeRecording  ( true   ),
              m_SerializeArtwork    ( true   ),
              m_SerializeCast       ( true   )
        {
        }
        
        Program( const Program &src )
        {
            Copy( src );
        }

        void Copy( const Program &src )
        {
            m_StartTime         = src.m_StartTime;
            m_EndTime           = src.m_EndTime;
            m_Title             = src.m_Title;
            m_SubTitle          = src.m_SubTitle;
            m_Category          = src.m_Category;
            m_CatType           = src.m_CatType;
            m_Repeat            = src.m_Repeat;
            m_SeriesId          = src.m_SeriesId;
            m_ProgramId         = src.m_ProgramId;
            m_Stars             = src.m_Stars;
            m_FileSize          = src.m_FileSize;
            m_LastModified      = src.m_LastModified;
            m_ProgramFlags      = src.m_ProgramFlags;
            m_VideoProps        = src.m_VideoProps;
            m_AudioProps        = src.m_AudioProps;
            m_SubProps          = src.m_SubProps;
            m_FileName          = src.m_FileName;
            m_HostName          = src.m_HostName;
            m_Airdate           = src.m_Airdate;
            m_Description       = src.m_Description;
            m_Inetref           = src.m_Inetref;
            m_Season            = src.m_Season;
            m_Episode           = src.m_Episode;
            m_TotalEpisodes     = src.m_TotalEpisodes;
            m_SerializeDetails  = src.m_SerializeDetails;
            m_SerializeChannel  = src.m_SerializeChannel;    
            m_SerializeRecording= src.m_SerializeRecording;  
            m_SerializeArtwork  = src.m_SerializeArtwork;
            m_SerializeCast     = src.m_SerializeCast;

            if ( src.m_Channel != NULL)
                Channel()->Copy( src.m_Channel );

            if ( src.m_Recording != NULL)
                Recording()->Copy( src.m_Recording );

            if ( src.m_Artwork != NULL)
                Artwork()->Copy( src.m_Artwork );

            if (src.m_Cast != NULL)
                Cast()->Copy( src.m_Cast );
        }

};

inline Program *ChannelInfo::AddNewProgram()
{
    // We must make sure the object added to the QVariantList has
    // a parent of 'this'

    Program *pObject = new Program( this );
    m_Programs.append( QVariant::fromValue<QObject *>( pObject ));

    return pObject;
}

} // namespace DTC

Q_DECLARE_METATYPE( DTC::Program  )
Q_DECLARE_METATYPE( DTC::Program* )

Q_DECLARE_METATYPE( DTC::ChannelInfo  )
Q_DECLARE_METATYPE( DTC::ChannelInfo* )

namespace DTC
{
inline void ChannelInfo::InitializeCustomTypes()
{
    qRegisterMetaType< ChannelInfo  >();
    qRegisterMetaType< ChannelInfo* >();

    if (QMetaType::type( "DTC::Program" ) == 0)
        Program::InitializeCustomTypes();
}

inline void Program::InitializeCustomTypes()
{
    qRegisterMetaType< Program  >();
    qRegisterMetaType< Program* >();

    if (QMetaType::type( "DTC::ChannelInfo" ) == 0)
        ChannelInfo::InitializeCustomTypes();

    if (QMetaType::type( "DTC::RecordingInfo" ) == 0)
        RecordingInfo::InitializeCustomTypes();

    if (QMetaType::type( "DTC::ArtworkInfoList" ) == 0)
        ArtworkInfoList::InitializeCustomTypes();

     if (QMetaType::type( "DTC::CastMemberList" ) == 0)
        CastMemberList::InitializeCustomTypes();
}
}

#endif

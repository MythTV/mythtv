//////////////////////////////////////////////////////////////////////////////
// Program Name: program.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <QDateTime>
#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "recording.h"

namespace DTC
{

class Program;


class SERVICE_PUBLIC ChannelInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.02" );

    // We need to know the type that will ultimately be contained in 
    // any QVariantList or QVariantMap.  We do his by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "Programs_type", "DTC::Program");

    Q_PROPERTY( uint      ChanId          READ ChanId         WRITE setChanId        )
    Q_PROPERTY( QString   ChanNum         READ ChanNum        WRITE setChanNum       )
    Q_PROPERTY( QString   CallSign        READ CallSign       WRITE setCallSign      )
    Q_PROPERTY( QString   IconURL         READ IconURL        WRITE setIconURL       )
    Q_PROPERTY( QString   ChannelName     READ ChannelName    WRITE setChannelName   )

    Q_PROPERTY( uint      MplexId         READ MplexId        WRITE setMplexId        DESIGNABLE SerializeDetails )
    Q_PROPERTY( uint      TransportId     READ TransportId    WRITE setTransportId    DESIGNABLE SerializeDetails )
    Q_PROPERTY( uint      ServiceId       READ ServiceId      WRITE setServiceId      DESIGNABLE SerializeDetails )
    Q_PROPERTY( uint      NetworkId       READ NetworkId      WRITE setNetworkId      DESIGNABLE SerializeDetails )
    Q_PROPERTY( uint      ATSCMajorChan   READ ATSCMajorChan  WRITE setATSCMajorChan  DESIGNABLE SerializeDetails )
    Q_PROPERTY( uint      ATSCMinorChan   READ ATSCMinorChan  WRITE setATSCMinorChan  DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   Format          READ Format         WRITE setFormat         DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   Modulation      READ Modulation     WRITE setModulation     DESIGNABLE SerializeDetails )
    Q_PROPERTY( long      Frequency       READ Frequency      WRITE setFrequency      DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   FrequencyId     READ FrequencyId    WRITE setFrequencyId    DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   FrequencyTable  READ FrequencyTable WRITE setFrequencyTable DESIGNABLE SerializeDetails )
    Q_PROPERTY( int       FineTune        READ FineTune       WRITE setFineTune       DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   SIStandard      READ SIStandard     WRITE setSIStandard     DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString   ChanFilters     READ ChanFilters    WRITE setChanFilters    DESIGNABLE SerializeDetails )
    Q_PROPERTY( int       SourceId        READ SourceId       WRITE setSourceId       DESIGNABLE SerializeDetails )
    Q_PROPERTY( int       InputId         READ InputId        WRITE setInputId        DESIGNABLE SerializeDetails )
    Q_PROPERTY( int       CommFree        READ CommFree       WRITE setCommFree       DESIGNABLE SerializeDetails )
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
    PROPERTYIMP       ( uint        , TransportId    )
    PROPERTYIMP       ( uint        , ServiceId      )
    PROPERTYIMP       ( uint        , NetworkId      )
    PROPERTYIMP       ( uint        , ATSCMajorChan  )
    PROPERTYIMP       ( uint        , ATSCMinorChan  )
    PROPERTYIMP       ( QString     , Format         )
    PROPERTYIMP       ( QString     , Modulation     )
    PROPERTYIMP       ( uint64_t    , Frequency      )
    PROPERTYIMP       ( QString     , FrequencyId    )
    PROPERTYIMP       ( QString     , FrequencyTable )
    PROPERTYIMP       ( int         , FineTune       )
    PROPERTYIMP       ( QString     , SIStandard     )
    PROPERTYIMP       ( QString     , ChanFilters    )
    PROPERTYIMP       ( int         , SourceId       )
    PROPERTYIMP       ( int         , InputId        )
    PROPERTYIMP       ( int         , CommFree       )
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
              m_SourceId        ( 0      ),
              m_InputId         ( 0      ),
              m_CommFree        ( 0      ),
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

            CopyListContents< Program >( this, m_Programs, src.m_Programs );
        }

        Program *AddNewProgram();

};

class SERVICE_PUBLIC Program : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );
    Q_CLASSINFO( "defaultProp", "Description" );

    Q_PROPERTY( QDateTime   StartTime    READ StartTime    WRITE setStartTime )
    Q_PROPERTY( QDateTime   EndTime      READ EndTime      WRITE setEndTime   )
    Q_PROPERTY( QString     Title        READ Title        WRITE setTitle     )
    Q_PROPERTY( QString     SubTitle     READ SubTitle     WRITE setSubTitle  )
    Q_PROPERTY( QString     Category     READ Category     WRITE setCategory  )
    Q_PROPERTY( QString     CatType      READ CatType      WRITE setCatType   )
    Q_PROPERTY( bool        Repeat       READ Repeat       WRITE setRepeat    )

    Q_PROPERTY( QString     SeriesId     READ SeriesId     WRITE setSeriesId     DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString     ProgramId    READ ProgramId    WRITE setProgramId    DESIGNABLE SerializeDetails )
    Q_PROPERTY( double      Stars        READ Stars        WRITE setStars        DESIGNABLE SerializeDetails )
    Q_PROPERTY( qlonglong   FileSize     READ FileSize     WRITE setFileSize     DESIGNABLE SerializeDetails )
    Q_PROPERTY( QDateTime   LastModified READ LastModified WRITE setLastModified DESIGNABLE SerializeDetails )
    Q_PROPERTY( int         ProgramFlags READ ProgramFlags WRITE setProgramFlags DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString     Hostname     READ Hostname     WRITE setHostname     DESIGNABLE SerializeDetails )
    Q_PROPERTY( QDate       Airdate      READ Airdate      WRITE setAirdate      DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString     Description  READ Description  WRITE setDescription  DESIGNABLE SerializeDetails )

    Q_PROPERTY( QObject*    Channel      READ Channel   DESIGNABLE SerializeChannel )
    Q_PROPERTY( QObject*    Recording    READ Recording DESIGNABLE SerializeRecording )

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
    PROPERTYIMP    ( QString     , Hostname     )
    PROPERTYIMP    ( QDate       , Airdate      )
    PROPERTYIMP    ( QString     , Description  )

    PROPERTYIMP_PTR( ChannelInfo  , Channel     )
    PROPERTYIMP_PTR( RecordingInfo, Recording   )

    // Used only by Serializer
    PROPERTYIMP( bool, SerializeDetails )
    PROPERTYIMP( bool, SerializeChannel )
    PROPERTYIMP( bool, SerializeRecording )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< Program  >();
            qRegisterMetaType< Program* >();

            if (QMetaType::type( "DTC::ChannelInfo" ) == 0)
                ChannelInfo::InitializeCustomTypes();

            if (QMetaType::type( "DTC::RecordingInfo" ) == 0)
                RecordingInfo::InitializeCustomTypes();
        }

    public:

        Program(QObject *parent = 0) 
            : QObject               ( parent ),
              m_Repeat              ( false  ),
              m_Stars               ( 0      ),
              m_FileSize            ( 0      ),
              m_ProgramFlags        ( 0      ),
              m_Channel             ( NULL   ),
              m_Recording           ( NULL   ),
              m_SerializeDetails    ( true   ),
              m_SerializeChannel    ( true   ),
              m_SerializeRecording  ( true   )
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
            m_Hostname          = src.m_Hostname;
            m_Airdate           = src.m_Airdate;
            m_Description       = src.m_Description;
            m_SerializeDetails  = src.m_SerializeDetails;
            m_SerializeChannel  = src.m_SerializeChannel;    
            m_SerializeRecording= src.m_SerializeRecording;  

            if ( src.m_Channel != NULL)
                Channel()->Copy( src.m_Channel );

            if ( src.m_Recording != NULL)
                Recording()->Copy( src.m_Recording );
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

inline void ChannelInfo::InitializeCustomTypes()
{
    qRegisterMetaType< ChannelInfo  >();
    qRegisterMetaType< ChannelInfo* >();

    if (QMetaType::type( "DTC::Program" ) == 0)
        Program::InitializeCustomTypes();
}


} // namespace DTC

Q_DECLARE_METATYPE( DTC::Program  )
Q_DECLARE_METATYPE( DTC::Program* )

Q_DECLARE_METATYPE( DTC::ChannelInfo  )
Q_DECLARE_METATYPE( DTC::ChannelInfo* )

#endif

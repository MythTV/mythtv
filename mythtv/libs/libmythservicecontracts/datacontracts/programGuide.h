//////////////////////////////////////////////////////////////////////////////
// Program Name: programGuide.h
// Created     : Jan. 15, 2010
//
// Purpose     : 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef PROGRAMGUIDE_H_
#define PROGRAMGUIDE_H_

#include <QDateTime>
#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "programAndChannel.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC ProgramGuide : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Channels", "type=DTC::ChannelInfo");
	Q_CLASSINFO( "AsOf"	   , "transient=true"       );

    Q_PROPERTY( QDateTime     StartTime      READ StartTime      WRITE setStartTime      )
    Q_PROPERTY( QDateTime     EndTime        READ EndTime        WRITE setEndTime        )
    Q_PROPERTY( int           StartChanId    READ StartChanId    WRITE setStartChanId    )
    Q_PROPERTY( int           EndChanId      READ EndChanId      WRITE setEndChanId      )
    Q_PROPERTY( int           NumOfChannels  READ NumOfChannels  WRITE setNumOfChannels  )
                                                                         
    Q_PROPERTY( bool          Details        READ Details        WRITE setDetails        )
    Q_PROPERTY( int           Count          READ Count          WRITE setCount          )
    Q_PROPERTY( QDateTime     AsOf           READ AsOf           WRITE setAsOf           )
    Q_PROPERTY( QString       Version        READ Version        WRITE setVersion        )
    Q_PROPERTY( QString       ProtoVer       READ ProtoVer       WRITE setProtoVer       )

    Q_PROPERTY( QVariantList Channels READ Channels DESIGNABLE true )

    PROPERTYIMP       ( QDateTime   , StartTime     )
    PROPERTYIMP       ( QDateTime   , EndTime       )
    PROPERTYIMP       ( int         , StartChanId   )
    PROPERTYIMP       ( int         , EndChanId     )
    PROPERTYIMP       ( int         , NumOfChannels )
                                                      
    PROPERTYIMP       ( bool        , Details       )
    PROPERTYIMP       ( int         , Count         )
    PROPERTYIMP       ( QDateTime   , AsOf          )
    PROPERTYIMP       ( QString     , Version       )
    PROPERTYIMP       ( QString     , ProtoVer      )

    PROPERTYIMP_RO_REF( QVariantList, Channels      )

    public:

        static inline void InitializeCustomTypes();

    public:

        ProgramGuide(QObject *parent = 0) 
            : QObject           ( parent ),
              m_StartChanId     ( 0      ),     
              m_EndChanId       ( 0      ),
              m_NumOfChannels   ( 0      ),
              m_Details         ( false  ),
              m_Count           ( 0      )  
        {
        }
        
        ProgramGuide( const ProgramGuide &src )
        {
            Copy( src );
        }

        void Copy( const ProgramGuide &src )
        {                                       
            m_StartTime    = src.m_StartTime    ;
            m_EndTime      = src.m_EndTime      ;
            m_StartChanId  = src.m_StartChanId  ;
            m_EndChanId    = src.m_EndChanId    ;
            m_NumOfChannels= src.m_NumOfChannels;
            m_Details      = src.m_Details      ;
            m_Count        = src.m_Count        ;
            m_AsOf         = src.m_AsOf         ;
            m_Version      = src.m_Version      ;
            m_ProtoVer     = src.m_ProtoVer     ;
        
            CopyListContents< ChannelInfo >( this, m_Channels, src.m_Channels );
        }

        ChannelInfo *AddNewChannel()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            ChannelInfo *pObject = new ChannelInfo( this );
            Channels().append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ProgramGuide  )
Q_DECLARE_METATYPE( DTC::ProgramGuide* )

namespace DTC
{
inline void ProgramGuide::InitializeCustomTypes()
{
    qRegisterMetaType< ProgramGuide  >();
    qRegisterMetaType< ProgramGuide* >();

    ChannelInfo::InitializeCustomTypes();
}
}

#endif

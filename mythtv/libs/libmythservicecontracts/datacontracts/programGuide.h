//////////////////////////////////////////////////////////////////////////////
// Program Name: programGuide.h
// Created     : Jan. 15, 2010
//
// Purpose     : 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
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
    Q_CLASSINFO( "AsOf"    , "transient=true"       );

    Q_PROPERTY( QDateTime     StartTime      READ StartTime      WRITE setStartTime      )
    Q_PROPERTY( QDateTime     EndTime        READ EndTime        WRITE setEndTime        )
    Q_PROPERTY( bool          Details        READ Details        WRITE setDetails        )

    Q_PROPERTY( int           StartIndex     READ StartIndex     WRITE setStartIndex     )
    Q_PROPERTY( int           Count          READ Count          WRITE setCount          )
    Q_PROPERTY( int           TotalAvailable READ TotalAvailable WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime     AsOf           READ AsOf           WRITE setAsOf           )
    Q_PROPERTY( QString       Version        READ Version        WRITE setVersion        )
    Q_PROPERTY( QString       ProtoVer       READ ProtoVer       WRITE setProtoVer       )

    Q_PROPERTY( QVariantList Channels READ Channels )

    PROPERTYIMP_REF   ( QDateTime   , StartTime     )
    PROPERTYIMP_REF   ( QDateTime   , EndTime       )
    PROPERTYIMP       ( bool        , Details       )

    PROPERTYIMP       ( int         , StartIndex    )
    PROPERTYIMP       ( int         , Count         )
    PROPERTYIMP       ( int         , TotalAvailable)
    PROPERTYIMP_REF   ( QDateTime   , AsOf          )
    PROPERTYIMP_REF   ( QString     , Version       )
    PROPERTYIMP_REF   ( QString     , ProtoVer      )

    PROPERTYIMP_RO_REF( QVariantList, Channels      )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE ProgramGuide(QObject *parent = nullptr)
            : QObject           ( parent ),
              m_Details         ( false  ),
              m_StartIndex      ( 0      ),
              m_Count           ( 0      ),
              m_TotalAvailable  ( 0      )
        {
        }

        void Copy( const ProgramGuide *src )
        {                                       
            m_StartTime      = src->m_StartTime      ;
            m_EndTime        = src->m_EndTime        ;
            m_Details        = src->m_Details        ;
            m_StartIndex     = src->m_StartIndex     ;
            m_Count          = src->m_Count          ;
            m_TotalAvailable = src->m_TotalAvailable ;
            m_AsOf           = src->m_AsOf           ;
            m_Version        = src->m_Version        ;
            m_ProtoVer       = src->m_ProtoVer       ;
        
            CopyListContents< ChannelInfo >( this, m_Channels, src->m_Channels );
        }

        ChannelInfo *AddNewChannel()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new ChannelInfo( this );
            Channels().append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(ProgramGuide);
};

inline void ProgramGuide::InitializeCustomTypes()
{
    qRegisterMetaType< ProgramGuide* >();

    ChannelInfo::InitializeCustomTypes();
}

} // namespace DTC

#endif

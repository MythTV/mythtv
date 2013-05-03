//////////////////////////////////////////////////////////////////////////////
// Program Name: encoder.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ENCODER_H_
#define ENCODER_H_

#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "programAndChannel.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Encoder : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( QString         HostName        READ HostName         WRITE setHostName       )
    Q_PROPERTY( bool            Local           READ Local            WRITE setLocal          )
    Q_PROPERTY( bool            Connected       READ Connected        WRITE setConnected      )
    Q_PROPERTY( int             State           READ State            WRITE setState          )
    Q_PROPERTY( int             SleepStatus     READ SleepStatus      WRITE setSleepStatus    )
    Q_PROPERTY( bool            LowOnFreeSpace  READ LowOnFreeSpace   WRITE setLowOnFreeSpace )

    Q_PROPERTY( QObject*        Recording       READ Recording  )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP    ( QString    , HostName       )
    PROPERTYIMP    ( bool       , Local          )
    PROPERTYIMP    ( bool       , Connected      )
    PROPERTYIMP    ( int        , State          )
    PROPERTYIMP    ( int        , SleepStatus    )
    PROPERTYIMP    ( bool       , LowOnFreeSpace )

    PROPERTYIMP_PTR( Program    , Recording      )

    public:

        static inline void InitializeCustomTypes();

    public:

        Encoder(QObject *parent = 0) 
            : QObject         ( parent ),
              m_Id            ( 0      ),
              m_Local         ( true   ),
              m_Connected     ( false  ),
              m_State         ( 0      ),
              m_SleepStatus   ( 0      ),
              m_LowOnFreeSpace( false  ),
              m_Recording     ( NULL   )  
        { 
        }
        
        Encoder( const Encoder &src )
        {
            Copy( src );
        }

        void Copy( const Encoder &src )
        {
            m_Id            = src.m_Id            ;
            m_HostName      = src.m_HostName      ;
            m_Local         = src.m_Local         ;
            m_Connected     = src.m_Connected     ;
            m_State         = src.m_State         ;
            m_SleepStatus   = src.m_SleepStatus   ;
            m_LowOnFreeSpace= src.m_LowOnFreeSpace;
            m_Recording     = NULL                ;
        
            if ( src.m_Recording != NULL)
                Recording()->Copy( *(src.m_Recording) );

        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::Encoder  )
Q_DECLARE_METATYPE( DTC::Encoder* )

namespace DTC
{
inline void Encoder::InitializeCustomTypes()
{
    qRegisterMetaType< Encoder  >();
    qRegisterMetaType< Encoder* >();

    Program::InitializeCustomTypes();
}
}

#endif

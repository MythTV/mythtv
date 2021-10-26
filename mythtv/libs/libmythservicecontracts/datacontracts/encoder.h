//////////////////////////////////////////////////////////////////////////////
// Program Name: encoder.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ENCODER_H_
#define ENCODER_H_

#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "input.h"
#include "programAndChannel.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Encoder : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_CLASSINFO( "Inputs", "type=DTC::Input");

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( QString         HostName        READ HostName         WRITE setHostName       )
    Q_PROPERTY( bool            Local           READ Local            WRITE setLocal          )
    Q_PROPERTY( bool            Connected       READ Connected        WRITE setConnected      )
    Q_PROPERTY( int             State           READ State            WRITE setState          )
    Q_PROPERTY( int             SleepStatus     READ SleepStatus      WRITE setSleepStatus    )
    Q_PROPERTY( bool            LowOnFreeSpace  READ LowOnFreeSpace   WRITE setLowOnFreeSpace )

    Q_PROPERTY( QVariantList    Inputs          READ Inputs     )
    Q_PROPERTY( QObject*        Recording       READ Recording  )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP_REF( QString    , HostName       )
    PROPERTYIMP    ( bool       , Local          )
    PROPERTYIMP    ( bool       , Connected      )
    PROPERTYIMP    ( int        , State          )
    PROPERTYIMP    ( int        , SleepStatus    )
    PROPERTYIMP    ( bool       , LowOnFreeSpace )

    PROPERTYIMP_PTR( Program    , Recording      )

    PROPERTYIMP_RO_REF( QVariantList, Inputs     );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE Encoder(QObject *parent = nullptr)
            : QObject         ( parent ),
              m_Id            ( 0      ),
              m_Local         ( true   ),
              m_Connected     ( false  ),
              m_State         ( 0      ),
              m_SleepStatus   ( 0      ),
              m_LowOnFreeSpace( false  ),
              m_Recording     ( nullptr )
        { 
        }

        void Copy( const Encoder *src )
        {
            m_Id            = src->m_Id            ;
            m_HostName      = src->m_HostName      ;
            m_Local         = src->m_Local         ;
            m_Connected     = src->m_Connected     ;
            m_State         = src->m_State         ;
            m_SleepStatus   = src->m_SleepStatus   ;
            m_LowOnFreeSpace= src->m_LowOnFreeSpace;
            m_Recording     = nullptr                ;
        
            if ( src->m_Recording != nullptr)
                Recording()->Copy( src->m_Recording );

            CopyListContents< Input >( this, m_Inputs, src->m_Inputs );
        }

        Input *AddNewInput()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new Input( this );
            Inputs().append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(Encoder);
};

inline void Encoder::InitializeCustomTypes()
{
    qRegisterMetaType< Encoder* >();

    Program::InitializeCustomTypes();
}

} // namespace DTC

#endif

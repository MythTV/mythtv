//////////////////////////////////////////////////////////////////////////////
// Program Name: encoder.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2ENCODER_H_
#define V2ENCODER_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

#include "v2input.h"
#include "v2programAndChannel.h"

/////////////////////////////////////////////////////////////////////////////

class V2Encoder : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    Q_CLASSINFO( "Inputs", "type=V2Input");

    SERVICE_PROPERTY2( int        , Id             )
    SERVICE_PROPERTY2( QString    , HostName       )
    SERVICE_PROPERTY2( bool       , Local          )
    SERVICE_PROPERTY2( bool       , Connected      )
    SERVICE_PROPERTY2( int        , State          )
    SERVICE_PROPERTY2( int        , SleepStatus    )
    SERVICE_PROPERTY2( bool       , LowOnFreeSpace )
    Q_PROPERTY( QObject*        Recording       READ Recording   USER true )
    SERVICE_PROPERTY_PTR( V2Program    , Recording      )
    SERVICE_PROPERTY2( QVariantList, Inputs     )

    public:

        Q_INVOKABLE V2Encoder(QObject *parent = nullptr)
            : QObject         ( parent ),
              m_Local         ( true   )
        {
        }

        void Copy( const V2Encoder *src )
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

            CopyListContents< V2Input >( this, m_Inputs, src->m_Inputs );
        }

        V2Input *AddNewInput()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Input( this );
            m_Inputs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2Encoder);
};

Q_DECLARE_METATYPE(V2Encoder*)

#endif

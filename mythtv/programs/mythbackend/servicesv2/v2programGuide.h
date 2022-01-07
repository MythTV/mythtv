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

#ifndef V2PROGRAMGUIDE_H_
#define V2PROGRAMGUIDE_H_

#include <QDateTime>
#include <QString>

#include "libmythbase/http/mythhttpservice.h"

#include "v2programAndChannel.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class V2ProgramGuide : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Channels", "type=V2ChannelInfo");
    Q_CLASSINFO( "AsOf"    , "transient=true"       );

    SERVICE_PROPERTY2   ( QDateTime   , StartTime     )
    SERVICE_PROPERTY2   ( QDateTime   , EndTime       )
    SERVICE_PROPERTY2   ( bool        , Details       )
    SERVICE_PROPERTY2   ( int         , StartIndex    )
    SERVICE_PROPERTY2   ( int         , Count         )
    SERVICE_PROPERTY2   ( int         , TotalAvailable)
    SERVICE_PROPERTY2   ( QDateTime   , AsOf          )
    SERVICE_PROPERTY2   ( QString     , Version       )
    SERVICE_PROPERTY2   ( QString     , ProtoVer      )
    SERVICE_PROPERTY2   ( QVariantList, Channels      )

    public:

        Q_INVOKABLE V2ProgramGuide(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2ProgramGuide *src )
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

            CopyListContents< V2ChannelInfo >( this, m_Channels, src->m_Channels );
        }

        V2ChannelInfo *AddNewChannel()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2ChannelInfo( this );
            m_Channels.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2ProgramGuide);
};

Q_DECLARE_METATYPE(V2ProgramGuide*)

#endif

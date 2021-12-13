//////////////////////////////////////////////////////////////////////////////
// Program Name: videoLookupInfoList.h
// Created     : Jul. 19, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2VIDEOLOOKUPINFOLIST_H_
#define V2VIDEOLOOKUPINFOLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2videoLookupInfo.h"


class V2VideoLookupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "VideoLookups", "type=V2VideoLookup");
    Q_CLASSINFO( "AsOf"        , "transient=true"       );


    SERVICE_PROPERTY2 ( int         , Count           )
    SERVICE_PROPERTY2 ( QDateTime   , AsOf            )
    SERVICE_PROPERTY2 ( QString     , Version         )
    SERVICE_PROPERTY2 ( QString     , ProtoVer        )
    SERVICE_PROPERTY2 ( QVariantList, VideoLookups );

    public:

        Q_INVOKABLE V2VideoLookupList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoLookupList *src )
        {
            m_Count         = src->m_Count          ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< V2VideoLookup >( this, m_VideoLookups, src->m_VideoLookups );
        }

        V2VideoLookup *AddNewVideoLookup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2VideoLookup( this );
            m_VideoLookups.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2VideoLookupList);
};

Q_DECLARE_METATYPE(V2VideoLookupList*)

#endif

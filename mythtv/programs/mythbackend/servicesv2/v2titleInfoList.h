////////////////////////////////////////////////////////////////////////////
// Program Name: titleInfoList.h
// Created     : June 14, 2013
//
// Copyright (c) 2013 Chris Pinkham <cpinkham@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
////////////////////////////////////////////////////////////////////////////

#ifndef V2TITLEINFOLIST_H_
#define V2TITLEINFOLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2titleInfo.h"

class V2TitleInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    Q_CLASSINFO( "TitleInfos", "type=V2TitleInfo");

    SERVICE_PROPERTY2( QVariantList, TitleInfos );

    public:

        Q_INVOKABLE V2TitleInfoList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2TitleInfoList *src )
        {
            CopyListContents< V2TitleInfo >( this, m_TitleInfos, src->m_TitleInfos );
        }

        V2TitleInfo *AddNewTitleInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2TitleInfo( this );
            m_TitleInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2TitleInfoList);
};

Q_DECLARE_METATYPE(V2TitleInfoList*)

#endif

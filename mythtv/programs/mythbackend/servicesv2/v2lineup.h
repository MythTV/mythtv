//////////////////////////////////////////////////////////////////////////////
// Program Name: lineup.h
// Created     : Sep. 18, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2LINEUP_H_
#define V2LINEUP_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"


/////////////////////////////////////////////////////////////////////////////

class V2Lineup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString    , LineupId       )
    SERVICE_PROPERTY2( QString    , Name           )
    SERVICE_PROPERTY2( QString    , DisplayName    )
    SERVICE_PROPERTY2( QString    , Type           )
    SERVICE_PROPERTY2( QString    , Postal         )
    SERVICE_PROPERTY2( QString    , Device         )

    public:

        Q_INVOKABLE V2Lineup(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const V2Lineup *src )
        {
            m_LineupId     = src->m_LineupId      ;
            m_Name         = src->m_Name          ;
            m_DisplayName  = src->m_DisplayName   ;
            m_Type         = src->m_Type          ;
            m_Postal       = src->m_Postal        ;
            m_Device       = src->m_Device        ;
        }

    private:
        Q_DISABLE_COPY(V2Lineup);
};

Q_DECLARE_METATYPE(V2Lineup*)

class V2LineupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    Q_CLASSINFO( "Lineups", "type=V2Lineup");

    SERVICE_PROPERTY2( QVariantList, Lineups );

    public:

        Q_INVOKABLE V2LineupList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2LineupList *src )
        {
            CopyListContents< V2Lineup >( this, m_Lineups, src->m_Lineups );
        }

        V2Lineup *AddNewLineup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Lineup( this );
            m_Lineups.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2LineupList);
};

Q_DECLARE_METATYPE(V2LineupList*)

#endif

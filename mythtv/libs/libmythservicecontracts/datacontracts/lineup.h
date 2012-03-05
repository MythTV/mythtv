//////////////////////////////////////////////////////////////////////////////
// Program Name: lineup.h
// Created     : Sep. 18, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef LINEUP_H_
#define LINEUP_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Lineup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString         LineupId        READ LineupId         WRITE setLineupId       )
    Q_PROPERTY( QString         Name            READ Name             WRITE setName           )
    Q_PROPERTY( QString         DisplayName     READ DisplayName      WRITE setDisplayName    )
    Q_PROPERTY( QString         Type            READ Type             WRITE setType           )
    Q_PROPERTY( QString         Postal          READ Postal           WRITE setPostal         )
    Q_PROPERTY( QString         Device          READ Device           WRITE setDevice         )

    PROPERTYIMP    ( QString    , LineupId       )
    PROPERTYIMP    ( QString    , Name           )
    PROPERTYIMP    ( QString    , DisplayName    )
    PROPERTYIMP    ( QString    , Type           )
    PROPERTYIMP    ( QString    , Postal         )
    PROPERTYIMP    ( QString    , Device         )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< Lineup  >();
            qRegisterMetaType< Lineup* >();
        }

    public:

        Lineup(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        Lineup( const Lineup &src )
        {
            Copy( src );
        }

        void Copy( const Lineup &src )
        {
            m_LineupId     = src.m_LineupId      ;
            m_Name         = src.m_Name          ;
            m_DisplayName  = src.m_DisplayName   ;
            m_Type         = src.m_Type          ;
            m_Postal       = src.m_Postal        ;
            m_Device       = src.m_Device        ;
        }
};

class SERVICE_PUBLIC LineupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // We need to know the type that will ultimately be contained in
    // any QVariantList or QVariantMap.  We do his by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "Lineups", "type=DTC::Lineup");

    Q_PROPERTY( QVariantList Lineups READ Lineups DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, Lineups )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< LineupList  >();
            qRegisterMetaType< LineupList* >();

            Lineup::InitializeCustomTypes();
        }

    public:

        LineupList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        LineupList( const LineupList &src )
        {
            Copy( src );
        }

        void Copy( const LineupList &src )
        {
            CopyListContents< Lineup >( this, m_Lineups, src.m_Lineups );
        }

        Lineup *AddNewLineup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            Lineup *pObject = new Lineup( this );
            m_Lineups.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::LineupList  )
Q_DECLARE_METATYPE( DTC::LineupList* )

Q_DECLARE_METATYPE( DTC::Lineup  )
Q_DECLARE_METATYPE( DTC::Lineup* )

#endif

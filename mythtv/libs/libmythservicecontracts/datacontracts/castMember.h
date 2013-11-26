//////////////////////////////////////////////////////////////////////////////
// Program Name: castMember.h
// Created     : Nov. 25, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef DTCCASTMEMBER_H_
#define DTCCASTMEMBER_H_

#include <QObject>
#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC CastMember : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "0.99" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_PROPERTY( QString   Name           READ Name           WRITE setName           )
    Q_PROPERTY( QString   CharacterName  READ CharacterName  WRITE setCharacterName  )
    Q_PROPERTY( QString   Role           READ Role           WRITE setRole           )
    Q_PROPERTY( QString   TranslatedRole READ TranslatedRole WRITE setTranslatedRole )

    PROPERTYIMP ( QString     , Name           )
    PROPERTYIMP ( QString     , CharacterName  )
    PROPERTYIMP ( QString     , Role           )
    PROPERTYIMP ( QString     , TranslatedRole )

    public:

        static void InitializeCustomTypes();

        CastMember(QObject *parent = 0)
            : QObject           ( parent )
        {
        }

        CastMember( const CastMember &src )
        {
            Copy( src );
        }

        void Copy( const CastMember &src )
        {
            m_Name           = src.m_Name          ;
            m_CharacterName  = src.m_CharacterName ;
            m_Role           = src.m_Role          ;
            m_TranslatedRole = src.m_TranslatedRole;
        }

};

}

Q_DECLARE_METATYPE( DTC::CastMember  )
Q_DECLARE_METATYPE( DTC::CastMember* )

namespace DTC
{
inline void CastMember::InitializeCustomTypes()
{
    qRegisterMetaType< CastMember   >();
    qRegisterMetaType< CastMember*  >();
}
}

#endif

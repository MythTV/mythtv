//////////////////////////////////////////////////////////////////////////////
// Program Name: castMember.h
// Created     : Nov. 25, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CASTMEMBER_H_
#define V2CASTMEMBER_H_

#include <QObject>
#include <QString>

#include "libmythbase/http/mythhttpservice.h"

class V2CastMember : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "0.99" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See mythhttpservice.h for details

    SERVICE_PROPERTY2 ( QString     , Name           )
    SERVICE_PROPERTY2 ( QString     , CharacterName  )
    SERVICE_PROPERTY2 ( QString     , Role           )
    SERVICE_PROPERTY2 ( QString     , TranslatedRole )

    public:

        Q_INVOKABLE V2CastMember(QObject *parent = nullptr)
            : QObject           ( parent )
        {
        }

        void Copy( const V2CastMember *src )
        {
            m_Name           = src->m_Name          ;
            m_CharacterName  = src->m_CharacterName ;
            m_Role           = src->m_Role          ;
            m_TranslatedRole = src->m_TranslatedRole;
        }

    private:
        Q_DISABLE_COPY(V2CastMember);
};

Q_DECLARE_METATYPE(V2CastMember*)

#endif

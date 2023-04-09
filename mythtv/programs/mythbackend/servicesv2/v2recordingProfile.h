//////////////////////////////////////////////////////////////////////////////
// Program Name: v2RecordingProfile.h
// Created     : Feb 28, 2023
//
// Copyright (c) 2023 Peter Bennett <pbennett@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2RECORDINGPROFILE_H_
#define V2RECORDINGPROFILE_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2RecProfParam : public QObject {
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString    ,     Name     )
    SERVICE_PROPERTY2( QString    ,     Value    )

    public:

    Q_INVOKABLE V2RecProfParam(QObject *parent = nullptr)
        : QObject          ( parent )
    {
    }

    private:
        Q_DISABLE_COPY(V2RecProfParam);
};

Q_DECLARE_METATYPE(V2RecProfParam*)



class V2RecProfile : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );
    Q_CLASSINFO( "RecProfParams", "type=V2RecProfParam");

    SERVICE_PROPERTY2( int        ,     Id         )
    SERVICE_PROPERTY2( QString    ,     Name       )
    SERVICE_PROPERTY2( QString    ,     VideoCodec )
    SERVICE_PROPERTY2( QString    ,     AudioCodec )
    SERVICE_PROPERTY2( QVariantList,    RecProfParams   )

    public:

        V2RecProfParam *AddParam()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2RecProfParam( this );
            m_RecProfParams.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }


    Q_INVOKABLE V2RecProfile(QObject *parent = nullptr)
        : QObject          ( parent )
    {
    }

    private:
        Q_DISABLE_COPY(V2RecProfile);
};

Q_DECLARE_METATYPE(V2RecProfile*)


class V2RecProfileGroup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );
    Q_CLASSINFO( "RecProfiles", "type=V2RecProfile");

    SERVICE_PROPERTY2( int        ,     Id       )
    SERVICE_PROPERTY2( QString    ,     Name       )
    SERVICE_PROPERTY2( QString    ,     CardType   )
    SERVICE_PROPERTY2( QVariantList,    RecProfiles )

    public:

        Q_INVOKABLE V2RecProfileGroup(QObject *parent = nullptr)
            : QObject          ( parent )
        {
        }

        V2RecProfile *AddProfile()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2RecProfile( this );
            m_RecProfiles.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2RecProfileGroup);
};

Q_DECLARE_METATYPE(V2RecProfileGroup*)

class V2RecProfileGroupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );
    Q_CLASSINFO( "RecProfileGroups", "type=V2RecProfileGroup");

    SERVICE_PROPERTY2( QVariantList,    RecProfileGroups )

    public:

        Q_INVOKABLE V2RecProfileGroupList(QObject *parent = nullptr)
            : QObject          ( parent )
        {
        }

        V2RecProfileGroup *AddProfileGroup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2RecProfileGroup( this );
            m_RecProfileGroups.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }



    private:
        Q_DISABLE_COPY(V2RecProfileGroupList);
};

Q_DECLARE_METATYPE(V2RecProfileGroupList*)



#endif

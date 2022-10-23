//////////////////////////////////////////////////////////////////////////////
// Program Name: v2grabber.h
// Created     : Sep 22, 2022
//
// Copyright (c) 2022 Peter Bennett <pbennett@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2GRABBER_H_
#define V2GRABBER_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2Grabber : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString    , Program      )
    SERVICE_PROPERTY2( QString    , DisplayName    )

    public:

        Q_INVOKABLE V2Grabber(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2Grabber *src )
        {
            m_Program       = src->m_Program;
            m_DisplayName   = src->m_DisplayName;
        }

    private:
        Q_DISABLE_COPY(V2Grabber);
};

Q_DECLARE_METATYPE(V2Grabber*)


class V2GrabberList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "Grabbers", "type=V2Grabber");

    SERVICE_PROPERTY2( QVariantList, Grabbers );

    public:

        Q_INVOKABLE V2GrabberList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        // Copy not needed
        // void Copy( const V2GrabberList *src )
        // {
        //     CopyListContents< V2Grabber >( this, m_Grabbers, src->m_Grabbers );
        // }

        V2Grabber *AddNewGrabber()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Grabber( this );
            m_Grabbers.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        bool containsProgram(QString &program)
        {
            auto matchProgram = [&program](const QVariant& entry)
            {
                auto *grabber = entry.value<V2Grabber*>();
                return (grabber->GetProgram() == program);
            };
            return std::any_of(m_Grabbers.cbegin(), m_Grabbers.cend(), matchProgram);
        }

    private:
        Q_DISABLE_COPY(V2GrabberList);
};

Q_DECLARE_METATYPE(V2GrabberList*)


#endif

//////////////////////////////////////////////////////////////////////////////
// Program Name: V2commMethod.h
// Created     : Feb. 12, 2023
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2COMMMETHOD_H_
#define V2COMMMETHOD_H_

#include <QObject>
#include <QString>

#include "libmythbase/http/mythhttpservice.h"

class  V2CommMethod : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );


    SERVICE_PROPERTY2( int         , CommMethod     )
    SERVICE_PROPERTY2( QString     , LocalizedName           );

    public:

        Q_INVOKABLE V2CommMethod(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        // void Copy( const V2CommMethod *src )
        // {
        //     m_CommMethod   = src->m_CommMethod     ;
        //     m_Name         = src->m_Name        ;
        // }

    private:
        Q_DISABLE_COPY(V2CommMethod);
};

Q_DECLARE_METATYPE(V2CommMethod*)

class V2CommMethodList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );
    Q_CLASSINFO( "CommMethods", "type=V2CommMethod");

    SERVICE_PROPERTY2( QVariantList, CommMethods );

    public:

        Q_INVOKABLE V2CommMethodList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        // void Copy( const V2CommMethodList *src )
        // {
        //     CopyListContents< V2Country >( this, m_CommMethods, src->m_CommMethods );
        // }

        V2CommMethod *AddNewCommMethod()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2CommMethod( this );
            m_CommMethods.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2CommMethodList);
};

Q_DECLARE_METATYPE(V2CommMethodList*)



#endif

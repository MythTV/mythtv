//////////////////////////////////////////////////////////////////////////////
// Program Name: cutList.h
// Created     : Mar. 09, 2014
//
// Copyright (c) 2014 team MythTV
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CUTLIST_H_
#define V2CUTLIST_H_

#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2cutting.h"

class V2CutList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "Cuttings", "type=V2Cutting");

    SERVICE_PROPERTY2( QVariantList, Cuttings );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE V2CutList(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

        void Copy( const V2CutList *src )
        {
            CopyListContents< V2Cutting >( this, m_Cuttings, src->m_Cuttings );
        }

        V2Cutting *AddNewCutting()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Cutting( this );
            m_Cuttings.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2CutList);
};

Q_DECLARE_METATYPE(V2CutList*)

#endif

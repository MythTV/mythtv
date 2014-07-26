//////////////////////////////////////////////////////////////////////////////
// Program Name: rtti.cpp
// Created     : July 25, 2014
//
// Copyright (c) 2014 David Blain <dblain@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "rtti.h"

#include <QCoreApplication>
#include <QMetaEnum>
#include <QStringList>

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Rtti::Rtti()
{
    RttiEnum::InitializeCustomTypes();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

RttiEnumList* Rtti::GetEnumDetails ( const QString &sFQN )
{
    if (sFQN.isEmpty())
        return NULL;

    // ----------------------------------------------------------------------
    // sEnumName needs to be in class.enum format
    // ----------------------------------------------------------------------

    if (sFQN.count('.') != 1 )
        return NULL;

    QStringList lstTypeParts = sFQN.split( '.' );

    // ----------------------------------------------------------------------
    // Create Parent object so we can get to its metaObject
    // ----------------------------------------------------------------------

    QString sParentFQN = "DTC::" + lstTypeParts[0];

    int nParentId = QMetaType::type( sParentFQN.toUtf8() );

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QObject *pParentClass = (QObject *)QMetaType::construct( nParentId );
#else
    QObject *pParentClass = (QObject *)QMetaType::create( nParentId );
#endif

    if (pParentClass == NULL)
        return NULL;

    const QMetaObject *pMetaObject = pParentClass->metaObject();

    QMetaType::destroy( nParentId, pParentClass );

    // ----------------------------------------------------------------------
    // Now look up enum
    // ----------------------------------------------------------------------

    int nEnumIdx = pMetaObject->indexOfEnumerator( lstTypeParts[1].toUtf8() );

    if (nEnumIdx < 0 )
        return NULL;

    QMetaEnum metaEnum = pMetaObject->enumerator( nEnumIdx );

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------

    RttiEnumList *pList = new RttiEnumList();

    for( int nIdx = 0; nIdx < metaEnum.keyCount(); nIdx++)
    {
        QString sKey   = metaEnum.key  ( nIdx );
        int     nValue = metaEnum.value( nIdx );

        QString sFQNKey = sFQN + "." + sKey;

        QString sDesc  = QCoreApplication::translate( "Enums", 
                                                      sFQNKey.toUtf8());

        RttiEnum *pRttiEnum = pList->AddNewRttiEnum();

        pRttiEnum->setKey  ( sKey   );
        pRttiEnum->setValue( nValue );
        pRttiEnum->setDesc ( sDesc  );

    }

    return pList;
}

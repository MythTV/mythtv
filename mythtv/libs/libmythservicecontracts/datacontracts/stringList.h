//////////////////////////////////////////////////////////////////////////////
// Program Name: stringList.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef STRINGLIST_H_
#define STRINGLIST_H_

#include <QStringList>

#include "serviceexp.h" 
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC StringList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_PROPERTY( QStringList Values READ Values DESIGNABLE true )

    PROPERTYIMP_RO_REF( QStringList, Values )

    public:

        StringList(QObject *parent = 0) 
            : QObject( parent )               
        {
        }
        
        StringList( const StringList &src ) 
            : m_Values( src.m_Values )        
        {
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::StringList )

#endif

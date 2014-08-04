//////////////////////////////////////////////////////////////////////////////
// Program Name: rtti.h
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

#ifndef RTTI_H
#define RTTI_H

#include "services/rttiServices.h"
#include "datacontracts/enum.h"

#include <QScriptEngine>

class Rtti : public RttiServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE Rtti( QObject *parent = 0 ) : RttiServices( parent ) {}

    public:

        DTC::Enum* GetEnum ( const QString   &FQN );

};

// --------------------------------------------------------------------------
// The following class wrapper is due to a limitation in Qt Script Engine.  It
// requires all methods that return pointers to user classes that are derived from
// QObject actually return QObject* (not the user class *).  If the user class pointer
// is returned, the script engine treats it as a QVariant and doesn't create a
// javascript prototype wrapper for it.
//
// This class allows us to keep the rich return types in the main API class while
// offering the script engine a class it can work with.
//
// Only API Classes that return custom classes needs to implement these wrappers.
//
// We should continue to look for a cleaning solution to this problem.
// --------------------------------------------------------------------------

class ScriptableRtti : public QObject
{
    Q_OBJECT

    private:

        Rtti    m_obj;

    public:

        Q_INVOKABLE ScriptableRtti( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:

        QObject* GetEnum  ( const QString   &FQN )
        {
            return m_obj.GetEnum( FQN );
        }
};

Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableRtti, QObject*);

#endif 

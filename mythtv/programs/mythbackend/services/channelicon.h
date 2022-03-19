//////////////////////////////////////////////////////////////////////////////
// Program Name: channelicon.h
// Created     : Jun. 22, 2014
//
// Copyright (c) 2014 The MythTV Team
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

#ifndef CHANNELICON_H
#define CHANNELICON_H

#include "mythconfig.h"
#if CONFIG_QTSCRIPT
#include <QScriptEngine>
#endif

#include "services/channelIconServices.h"

class ChannelIcon : public ChannelIconServices
{
    Q_OBJECT

    public:

        Q_INVOKABLE explicit ChannelIcon( QObject *parent = nullptr ) {}

    public:

        /* ChannelIcon Methods */

        DTC::ChannelIconList*       LookupChannelIcon ( const QString    &Query,
                                                        const QString    &FieldName   );

        DTC::ChannelIconList*       SearchChannelIcon ( const QString    &Query       );

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

#if CONFIG_QTSCRIPT
class ScriptableChannelIcon : public QObject
{
    Q_OBJECT

    private:

        ChannelIcon    m_obj;

    public:

        Q_INVOKABLE explicit ScriptableChannelIcon( QObject *parent = nullptr ) : QObject( parent ) {}

    public slots:

        QObject* LookupChannelIcon ( const QString    &Query,
                                     const QString    &FieldName   )
        {
            return m_obj.LookupChannelIcon( Query, FieldName );
        }

        QObject* SearchChannelIcon ( const QString    &Query       )
        {
            return m_obj.SearchChannelIcon( Query );
        }
};


Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableChannelIcon, QObject*);
#endif

#endif

//////////////////////////////////////////////////////////////////////////////
// Program Name: dvr.h
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
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

#ifndef DVR_H
#define DVR_H

#include <QScriptEngine>

#include "services/dvrServices.h"

class Dvr : public DvrServices
{
    Q_OBJECT

    public:
    
        Q_INVOKABLE Dvr( QObject *parent = 0 ) {}

    public:

        DTC::ProgramList* GetExpiring         ( int              StartIndex, 
                                                int              Count      );

        DTC::ProgramList* GetRecorded         ( bool             Descending,
                                                int              StartIndex,
                                                int              Count      );

        DTC::EncoderList* Encoders            ( );
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

class ScriptableDvr : public QObject
{
    Q_OBJECT

    private:

        Dvr    m_obj;

    public:
    
        Q_INVOKABLE ScriptableDvr( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:

        QObject* GetExpiring         ( int              StartIndex, 
                                       int              Count      )
        {
            return m_obj.GetExpiring( StartIndex, Count );
        }

        QObject* GetRecorded         ( bool             Descending,
                                       int              StartIndex,
                                       int              Count      )
        {
            return m_obj.GetRecorded( Descending, StartIndex, Count );
        }

        QObject* Encoders            () { return m_obj.Encoders(); }


};

Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableDvr, QObject*);

#endif 

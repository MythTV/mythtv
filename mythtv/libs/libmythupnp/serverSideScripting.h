//////////////////////////////////////////////////////////////////////////////
// Program Name: serverSideScripting.h
// Created     : Mar. 22, 2011
//
// Purpose     : Server Side Scripting support for Html Server
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

#ifndef SERVERSIDESCRIPTING_H_
#define SERVERSIDESCRIPTING_H_

#include "upnpexp.h"

#include <QString> 
#include <QMap>
#include <QDateTime>
#include <QMutex>
#include <QTextStream> 
#include <QScriptEngine>
#include <QScriptable>

class ScriptInfo 
{
  public:
    QScriptValue    m_oFunc;
    QDateTime       m_dtTimeStamp;

    ScriptInfo() {}

    ScriptInfo( QScriptValue func, QDateTime dt )
        : m_oFunc( func ), m_dtTimeStamp( dt )
    {}
};

class UPNP_PUBLIC ServerSideScripting
{
    protected:

        QMutex                          m_mutex;
        QMap< QString, ScriptInfo* >     m_mapScripts;

        void Lock       () { m_mutex.lock();   }
        void Unlock     () { m_mutex.unlock(); }

    public:

         QScriptEngine                   m_engine;

    public:

         ServerSideScripting();
        ~ServerSideScripting();

        void RegisterMetaObjectType( const QString     &sName, 
                                     const QMetaObject *pMetaObject,
                                     QScriptEngine::FunctionSignature  pFunction);

        bool EvaluatePage( QTextStream *pOutStream, const QString &sFileName );

    protected:

        QString CreateMethodFromFile( const QString &sFileName );

        bool    ProcessLine         ( QTextStream &sCode, 
                                      QString     &sLine, 
                                      bool         bInCode,
                                      QString     &sTransBuffer );
};


class OutputStream : public QObject, public QScriptable
{
    Q_OBJECT

    public:

         OutputStream( QTextStream *pStream, QObject *parent = 0 )
             : QObject( parent ), m_pTextStream( pStream )  {}

         ~OutputStream() {} 

    public slots:

        void write( const QString &sValue )
        {
            *m_pTextStream << sValue;
        }

        void writeln( const QString &sValue )
        {
            *m_pTextStream << sValue << "\n";
        }

    private:
        QTextStream *m_pTextStream;
};

#endif

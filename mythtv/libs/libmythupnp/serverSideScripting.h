//////////////////////////////////////////////////////////////////////////////
// Program Name: serverSideScripting.h
// Created     : Mar. 22, 2011
//
// Purpose     : Server Side Scripting support for Html Server
//                                                                            
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SERVERSIDESCRIPTING_H_
#define SERVERSIDESCRIPTING_H_

#include <utility>

// Qt headers
#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QScriptEngine>
#include <QScriptable>
#include <QString>
#include <QTextStream>

// MythTV headers
#include "upnpexp.h"
#include "httprequest.h"

#ifdef _WIN32
#include <QScriptEngineDebugger>
#endif

class ScriptInfo 
{
  public:
    QScriptValue    m_oFunc;
    QDateTime       m_dtTimeStamp;

    ScriptInfo() = default;

    ScriptInfo( const QScriptValue& func, QDateTime dt )
        : m_oFunc( func ), m_dtTimeStamp(std::move( dt ))
    {}
};

class UPNP_PUBLIC ServerSideScripting
{
    public:

        QScriptEngine                   m_engine;

#ifdef _WIN32
        QScriptEngineDebugger           m_debugger;
#endif

        ServerSideScripting();
       ~ServerSideScripting();

        QString SetResourceRootPath( const QString     &path );

        void RegisterMetaObjectType( const QString     &sName, 
                                     const QMetaObject *pMetaObject,
                                     QScriptEngine::FunctionSignature  pFunction);

        bool EvaluatePage( QTextStream *pOutStream, const QString &sFileName,
                           HTTPRequest *pRequest, const QByteArray &cspToken);

    protected:

        QMutex                           m_mutex;
        QMap< QString, ScriptInfo* >     m_mapScripts;
        QString                          m_sResRootPath;

        void Lock       () { m_mutex.lock();   }
        void Unlock     () { m_mutex.unlock(); }

        ScriptInfo *GetLoadedScript ( const QString &sFileName );

        static QString ReadFileContents    ( const QString &sFileName ) ;

        QString CreateMethodFromFile( const QString &sFileName ) const;

        bool    ProcessLine         ( QTextStream &sCode, 
                                      QString     &sLine, 
                                      bool         bInCode,
                                      QString     &sTransBuffer ) const;
};


class OutputStream : public QObject, public QScriptable
{
    Q_OBJECT

    public:

         explicit OutputStream( QTextStream *pStream, QObject *parent = nullptr )
             : QObject( parent ), m_pTextStream( pStream )  {}

         ~OutputStream() override = default;

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
        QTextStream *m_pTextStream {nullptr};
};

#endif

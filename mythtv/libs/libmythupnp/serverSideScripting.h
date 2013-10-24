//////////////////////////////////////////////////////////////////////////////
// Program Name: serverSideScripting.h
// Created     : Mar. 22, 2011
//
// Purpose     : Server Side Scripting support for Html Server
//                                                                            
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SERVERSIDESCRIPTING_H_
#define SERVERSIDESCRIPTING_H_

#include "upnpexp.h"
#include "upnputil.h"

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

        QMutex                           m_mutex;
        QMap< QString, ScriptInfo* >     m_mapScripts;
        QString                          m_sResRootPath;

        void Lock       () { m_mutex.lock();   }
        void Unlock     () { m_mutex.unlock(); }

    public:

         QScriptEngine                   m_engine;

    public:

         ServerSideScripting();
        ~ServerSideScripting();

        QString SetResourceRootPath( const QString     &path );

        void RegisterMetaObjectType( const QString     &sName, 
                                     const QMetaObject *pMetaObject,
                                     QScriptEngine::FunctionSignature  pFunction);

        bool EvaluatePage( QTextStream *pOutStream, const QString &sFileName,
                           const QStringMap &mapParams );

    protected:

        ScriptInfo *GetLoadedScript ( const QString &sFileName );

        QString ReadFileContents    ( const QString &sFileName );

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

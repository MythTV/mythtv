//////////////////////////////////////////////////////////////////////////////
// Program Name: serverSideScripting.cpp
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

#include <QFile>
#include <QFileInfo>

#include "serverSideScripting.h"
#include "mythverbose.h"

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

ServerSideScripting::ServerSideScripting()
{
    // ----------------------------------------------------------------------
    // Enable Translation functions
    // ----------------------------------------------------------------------

    m_engine.installTranslatorFunctions();

    // ----------------------------------------------------------------------
    // Add Scriptable Objects
    // ----------------------------------------------------------------------

    // Q_SCRIPT_DECLARE_QMETAOBJECT( DTC::MythService, QObject*)
    // QScriptValue oClass = engine.scriptValueFromQMetaObject< DTC::MythService >();
    // engine.globalObject().setProperty("Myth", oClass);
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

ServerSideScripting::~ServerSideScripting()
{
    Lock();

    QMap<QString, ScriptInfo*>::iterator it = m_mapScripts.begin();

    for (; it != m_mapScripts.end(); ++it)
    {
        if (*it)
            delete (*it);
    }

    m_mapScripts.clear();
    Unlock();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void ServerSideScripting::RegisterMetaObjectType( const QString &sName, 
                                                  const QMetaObject *pMetaObject,
                                                  QScriptEngine::FunctionSignature  pFunction)
{
    QScriptValue ctor = m_engine.newFunction( pFunction );

    QScriptValue metaObject = m_engine.newQMetaObject( pMetaObject, ctor );
    m_engine.globalObject().setProperty( sName, metaObject );
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool ServerSideScripting::EvaluatePage( QTextStream *pOutStream, const QString &sFileName )
{
    try
    {
        bool       bFound( false     );
        ScriptInfo *pInfo = NULL;

        // ------------------------------------------------------------------
        // See if page has already been loaded
        // ------------------------------------------------------------------

        Lock();

        if ( (bFound = m_mapScripts.contains( sFileName )) == true )
            pInfo = m_mapScripts[ sFileName ];

        Unlock();

        // ------------------------------------------------------------------
        // Load Script File and Create Function
        // ------------------------------------------------------------------

        QFileInfo  fileInfo ( sFileName );
        QDateTime  dtLastModified = fileInfo.lastModified();

        if ((pInfo == NULL) || (pInfo->m_dtTimeStamp != dtLastModified ))
        {
            QString      sCode = CreateMethodFromFile( sFileName );

            QScriptValue func  = m_engine.evaluate( sCode, sFileName );

            if ( m_engine.hasUncaughtException() )
            {
                VERBOSE( VB_IMPORTANT, QString( "Error Loading QSP File: %1 - (%2)%3" )
                                          .arg( sFileName )
                                          .arg( m_engine.uncaughtExceptionLineNumber() )
                                          .arg( m_engine.uncaughtException().toString() ));

                return false;
            }

            if (pInfo != NULL)
            {
                pInfo->m_oFunc       = func;
                pInfo->m_dtTimeStamp = dtLastModified;
            }
            else
            {
                pInfo = new ScriptInfo( func, dtLastModified );
                Lock();
                m_mapScripts[ sFileName ] = pInfo;
                Unlock();
            }
        }

        // ------------------------------------------------------------------
        // Execute function to render output
        // ------------------------------------------------------------------

        OutputStream outStream( pOutStream );

        QScriptValueList args;
        args << m_engine.newQObject( &outStream );

        pInfo->m_oFunc.call( QScriptValue(), args );

        if (m_engine.hasUncaughtException())
        {
            VERBOSE( VB_IMPORTANT, QString( "Error calling QSP File: %1 - %2" )
                                      .arg( sFileName )
                                      .arg( m_engine.uncaughtException().toString() ));
            return false;
        }

    }
    catch( ... )
    {
        VERBOSE( VB_IMPORTANT, QString( "Exception while evaluating QSP File: %1" )
                                  .arg( sFileName ));

        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString ServerSideScripting::CreateMethodFromFile( const QString &sFileName )
{
    bool        bInCode = false;
    QString     sBuffer;
    QTextStream sCode( &sBuffer );
        
    QFile   scriptFile( sFileName );

    if (!scriptFile.open( QIODevice::ReadOnly ))
        throw "Unable to open file";

    try
    {
        QTextStream stream( &scriptFile );

        sCode << "(function( os ) {\n";

        while( !stream.atEnd() )
        {
            QString sLine = stream.readLine();

            bInCode = ProcessLine( sCode, sLine, bInCode );
        }
    
        sCode << "})";
    }
    catch( ... )
    {
        VERBOSE( VB_IMPORTANT, QString( "Exception while reading QSP File: %1" )
                                  .arg( sFileName ));
    }

    scriptFile.close();

    sCode.flush();

    return sBuffer;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool ServerSideScripting::ProcessLine( QTextStream &sCode, 
                                       QString     &sLine, 
                                       bool         bInCode )
{
    int  nStartPos       = 0;
    int  nEndPos         = 0;
    int  nMatchPos       = 0;
    bool bMatchFound     = false;

    sLine = sLine.trimmed();

    QString sExpecting = bInCode ? "%>" : "<%";
    bool    bNewLine   = !(sLine.startsWith( sExpecting ));
        
    while (nStartPos < sLine.length())
    {
        nEndPos = sLine.length() - 1;
        
        sExpecting = bInCode ? "%>" : "<%";
        nMatchPos  = sLine.indexOf( sExpecting, nStartPos );
    
        // ------------------------------------------------------------------
        // If not found, Adjust to Save entire line
        // ------------------------------------------------------------------
        
        if (nMatchPos < 0)
        {
            nMatchPos = nEndPos + 1;
            bMatchFound = false;
        }
        else
            bMatchFound = true;
        
        // ------------------------------------------------------------------
        // Add Code or Text to Line
        // ------------------------------------------------------------------
        
        QString sSegment = sLine.mid( nStartPos, nMatchPos - nStartPos );
                
        if ( !sSegment.isEmpty())
        {
            if (bInCode)
            {
                // Add Code
    
                if (sSegment.startsWith( "=" ))
                    sCode << "os.write( " << sSegment.mid( 1 ) << " ); " << "\n";
                else
                    sCode << sSegment << "\n";

                if (bMatchFound)
                    bInCode = false;
            }
            else
            {
                // Add Text
                
                sSegment.replace( '"', "\\\"" );
    
                sCode << "os.write( \"" << sSegment << "\" );\n";

                if (bMatchFound) 
                    bInCode = true;
                else
                    bNewLine = false;
            }
        }
        else
        {
            if (bMatchFound)
                bInCode = !bInCode;
        }
        
        nStartPos = nMatchPos + 2;
    }        

    if ((bNewLine) && !bInCode )
        sCode << "os.writeln( \"\" );\n";
    
    return bInCode;
}

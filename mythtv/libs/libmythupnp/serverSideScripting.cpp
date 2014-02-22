//////////////////////////////////////////////////////////////////////////////
// Program Name: serverSideScripting.cpp
// Created     : Mar. 22, 2011
//
// Purpose     : Server Side Scripting support for Html Server
//                                                                            
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QVariant>

#include "serverSideScripting.h"
#include "mythlogging.h"
#include "httpserver.h"

QScriptValue formatStr(QScriptContext *context, QScriptEngine *interpreter);

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QScriptValue formatStr(QScriptContext *context, QScriptEngine *interpreter)
{
    unsigned int count = context->argumentCount();

    if (count == 0)
        return QScriptValue(interpreter, QString());
 
    if (count == 1)
        return QScriptValue(interpreter, context->argument(0).toString());

    QString result = context->argument(0).toString();
    for (unsigned int i = 1; i < count; i++)
        result.replace(QString("%%1").arg(i), context->argument(i).toString());

    return QScriptValue(interpreter, result);
}

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
    // Register C++ functions
    // ----------------------------------------------------------------------

    QScriptValue qsFormatStr = m_engine.newFunction(formatStr);
    m_engine.globalObject().setProperty("formatStr", qsFormatStr);

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

QString ServerSideScripting::SetResourceRootPath( const QString &path )
{
    QString sOrig = m_sResRootPath;

    m_sResRootPath = path;

    return sOrig;
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

ScriptInfo *ServerSideScripting::GetLoadedScript( const QString &sFileName )
{
    ScriptInfo *pInfo = NULL;

    Lock();

    if ( m_mapScripts.contains( sFileName ) )
        pInfo = m_mapScripts[ sFileName ];

    Unlock();

    return pInfo;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool ServerSideScripting::EvaluatePage( QTextStream *pOutStream, const QString &sFileName,
                                        HTTPRequest *pRequest)
{
    try
    {
        ScriptInfo *pInfo = NULL;

        // ------------------------------------------------------------------
        // See if page has already been loaded
        // ------------------------------------------------------------------

        pInfo = GetLoadedScript( sFileName );

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
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Error Loading QSP File: %1 - (line %2) %3")
                        .arg(sFileName)
                        .arg(m_engine.uncaughtExceptionLineNumber())
                        .arg(m_engine.uncaughtException().toString()));

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
        // Build array of arguments passed to script
        // ------------------------------------------------------------------

        QStringMap mapParams = pRequest->m_mapParams;

        // Valid characters for object property names must contain only
        // word characters and numbers, _ and $
        // They must not start with a number - to simplify the regexp, we
        // restrict the first character to the English alphabet
        QRegExp validChars = QRegExp("^([a-zA-Z]|_|\\$)(\\w|\\$)+$");

        QVariantMap params;
        QMap<QString, QString>::const_iterator it = mapParams.begin();
        QString prevArrayName = "";
        QVariantMap array;
        for (; it != mapParams.end(); ++it)
        {
            QString key = it.key();
            QVariant value = QVariant(it.value());

            // PHP Style parameter array
            if (key.contains("["))
            {
                QString arrayName = key.section('[',0,0);
                QString arrayKey = key.section('[',1,1);
                arrayKey.chop(1); // Remove trailing ]
                if (prevArrayName != arrayName) // New or different array
                {
                    if (!array.isEmpty())
                    {
                        params.insert(prevArrayName, QVariant(array));
                        array.clear();
                    }
                    prevArrayName = arrayName;
                }

                if (!validChars.exactMatch(arrayKey)) // Discard anything that isn't valid for now
                    continue;

                array.insert(arrayKey, value);

                if ((it + 1) != mapParams.end())
                    continue;
            }

            if (!array.isEmpty())
            {
                params.insert(prevArrayName, QVariant(array));
                array.clear();
            }
            // End Array handling

            if (!validChars.exactMatch(key)) // Discard anything that isn't valid for now
                continue;

            params.insert(key, value);

        }

        // ------------------------------------------------------------------
        // Build array of request headers
        // ------------------------------------------------------------------

        QStringMap mapHeaders = pRequest->m_mapHeaders;

        QVariantMap requestHeaders;
        for (it = mapHeaders.begin(); it != mapHeaders.end(); ++it)
        {
            QString key = it.key();
            key = key.replace('-', '_'); // May be other valid chars in a request header that we need to replace
            QVariant value = QVariant(it.value());

            if (!validChars.exactMatch(key)) // Discard anything that isn't valid for now
                continue;

            requestHeaders.insert(key, value);
        }

        // ------------------------------------------------------------------
        // Build array of information from the server e.g. client IP
        // See RFC 3875 - The Common Gateway Interface
        // ------------------------------------------------------------------

        QVariantMap serverVars;
        serverVars.insert("REMOTE_ADDR", QVariant(pRequest->GetPeerAddress()));
        serverVars.insert("SERVER_ADDR", QVariant(pRequest->GetHostAddress()));
        serverVars.insert("SERVER_PROTOCOL", QVariant(pRequest->GetRequestProtocol()));
        serverVars.insert("SERVER_SOFTWARE", QVariant(HttpServer::GetServerVersion()));

        QHostAddress clientIP = QHostAddress(pRequest->GetPeerAddress());
        QHostAddress serverIP = QHostAddress(pRequest->GetHostAddress());
        if (clientIP.protocol() == QAbstractSocket::IPv4Protocol)
        {
            serverVars.insert("IP_PROTOCOL", "IPv4");
        }
        else if (clientIP.protocol() == QAbstractSocket::IPv6Protocol)
        {
            serverVars.insert("IP_PROTOCOL", "IPv6");
        }

        if (((clientIP.protocol() == QAbstractSocket::IPv4Protocol) &&
             (clientIP.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
              clientIP.isInSubnet(QHostAddress("192.168.0.0"), 16) ||
              clientIP.isInSubnet(QHostAddress("10.0.0.0"), 8))) ||
            ((clientIP.protocol() == QAbstractSocket::IPv6Protocol) &&
              clientIP.isInSubnet(serverIP, 64))) // default subnet size is assumed to be /64
        {
            serverVars.insert("CLIENT_NETWORK", "local");
        }
        else
        {
            serverVars.insert("CLIENT_NETWORK", "remote");
        }

        // ------------------------------------------------------------------
        // Add the arrays (objects) we've just created to the global scope
        // They may be accessed as 'Server.REMOTE_ADDR'
        // ------------------------------------------------------------------

        m_engine.globalObject().setProperty("Parameters",
                                            m_engine.toScriptValue(params));
        m_engine.globalObject().setProperty("RequestHeaders",
                                            m_engine.toScriptValue(requestHeaders));
        m_engine.globalObject().setProperty("Server",
                                            m_engine.toScriptValue(serverVars));

        // ------------------------------------------------------------------
        // Execute function to render output
        // ------------------------------------------------------------------

        OutputStream outStream( pOutStream );

        QScriptValueList args;
        args << m_engine.newQObject( &outStream );
        args << m_engine.toScriptValue(params);

        pInfo->m_oFunc.call( QScriptValue(), args );

        if (m_engine.hasUncaughtException())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error calling QSP File: %1 - %2")
                    .arg(sFileName)
                    .arg(m_engine.uncaughtException().toString()));
            return false;
        }
    }
    catch(...)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Exception while evaluating QSP File: %1") .arg(sFileName));

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
        QString sTransBuffer;

        sCode << "(function( os, ARGS ) {\n";

        while( !stream.atEnd() )
        {
            QString sLine = stream.readLine();

            bInCode = ProcessLine( sCode, sLine, bInCode, sTransBuffer );
        }
    
        sCode << "})";
    }
    catch(...)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Exception while reading QSP File: %1") .arg(sFileName));
    }

    scriptFile.close();

    sCode.flush();

    return sBuffer;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString ServerSideScripting::ReadFileContents( const QString &sFileName )
{
    QString  sCode;
    QFile    scriptFile( sFileName );

    if (!scriptFile.open( QIODevice::ReadOnly ))
        throw "Unable to open file";

    try
    {
        QTextStream stream( &scriptFile );

        sCode = stream.readAll();
    }
    catch(...)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Exception while Reading File Contents File: %1") .arg(sFileName));
    }

    scriptFile.close();

    return sCode;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

bool ServerSideScripting::ProcessLine( QTextStream &sCode, 
                                       QString     &sLine, 
                                       bool         bInCode,
                                       QString     &sTransBuffer )
{
    QString sLowerLine = sLine.toLower();

    if (!sTransBuffer.isEmpty())
    {
        int nEndTransPos = sLowerLine.indexOf("</i18n>");

        if (nEndTransPos == -1)
        {
            sTransBuffer.append(" ");
            sTransBuffer.append(sLine);
            return bInCode;
        }

        if (nEndTransPos > 0)
            sTransBuffer.append(" ");

        sTransBuffer.append(sLine.left(nEndTransPos).trimmed());
        QString trStr =
            QCoreApplication::translate("HtmlUI", sTransBuffer.trimmed().toLocal8Bit().data());
        trStr.replace( '"', "\\\"" );
        sCode << "os.write( \"" << trStr << "\" );\n";
        sTransBuffer = "";

        if (nEndTransPos == (sLine.length() - 7))
            return bInCode;

        sLine = sLine.right(sLine.length() - (nEndTransPos + 7));
    }

    int nStartTransPos = sLowerLine.indexOf("<i18n>");
    if (nStartTransPos != -1)
    {
        int nEndTransPos = sLowerLine.indexOf("</i18n>");
        if (nEndTransPos != -1)
        {
            QString patStr = sLine.mid(nStartTransPos,
                                       (nEndTransPos + 7 - nStartTransPos));
            QString repStr = patStr.mid(6, patStr.length() - 13).trimmed();
            sLine.replace(patStr, QCoreApplication::translate("HtmlUI", repStr.toLocal8Bit().data()));
            return ProcessLine(sCode, sLine, bInCode, sTransBuffer);
        }
        else
        {
            sTransBuffer = " ";
            sTransBuffer.append(sLine.mid(nStartTransPos + 6).trimmed());
            sLine = sLine.left(nStartTransPos);
        }
    }

    int  nStartPos       = 0;
    int  nEndPos         = 0;
    int  nMatchPos       = 0;
    bool bMatchFound     = false;

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
                {
                    // Evaluate statement and render results.

                    sCode << "os.write( " << sSegment.mid( 1 ) << " ); "
                          << "\n";
                }
                else if (sSegment.startsWith( "import" ))
                {
                    // Loads supplied path as script file. 
                    //
                    // Syntax: import "/relative/path/to/script.js"
                    //   - must be at start of line (no leading spaces)
                    //

                    // Extract filename (remove quotes)

                    QStringList sParts = sSegment.split( ' ', QString::SkipEmptyParts );

                    if (sParts.length() > 1 )
                    {
                        QString sFileName = sParts[1].mid( 1, sParts[1].length() - 2 );

                        QFileInfo oInfo( m_sResRootPath + sFileName );

                        if (oInfo.exists())
                        {
                            sCode << ReadFileContents( oInfo.canonicalFilePath() )
                                  << "\n";
                        }
                        else
                            LOG(VB_GENERAL, LOG_ERR,
                                QString("ServerSideScripting::ProcessLine 'import' - File not found: %1%2")
                                   .arg(m_sResRootPath)
                                   .arg(sFileName));
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("ServerSideScripting::ProcessLine 'import' - Malformed [%1]")
                                .arg( sSegment ));
                    }

                }
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

//////////////////////////////////////////////////////////////////////////////
// Program Name: serverSideScripting.cpp
// Created     : Mar. 22, 2011
//
// Purpose     : Server Side Scripting support for Html Server
//                                                                            
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "serverSideScripting.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QVariant>
#include <QVariantMap>

#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsession.h"

#include "httpserver.h"

QScriptValue formatStr(QScriptContext *context, QScriptEngine *interpreter);

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QScriptValue formatStr(QScriptContext *context, QScriptEngine *interpreter)
{
    unsigned int count = context->argumentCount();

    if (count == 0)
        return {interpreter, QString()};
 
    if (count == 1)
        return {interpreter, context->argument(0).toString()};

    QString result = context->argument(0).toString();
    for (unsigned int i = 1; i < count; i++)
        result.replace(QString("%%1").arg(i), context->argument(i).toString());

    return {interpreter, result};
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

ServerSideScripting::ServerSideScripting()
{
    Lock();

#ifdef _WIN32
    m_debugger.attachTo( &m_engine );
#endif

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
    Unlock();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

ServerSideScripting::~ServerSideScripting()
{
    Lock();

    for (const auto *script : std::as_const(m_mapScripts))
        delete script;

    m_mapScripts.clear();
    Unlock();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString ServerSideScripting::SetResourceRootPath( const QString &path )
{
    Lock();
    QString sOrig = m_sResRootPath;

    m_sResRootPath = path;
    Unlock();

    return sOrig;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

void ServerSideScripting::RegisterMetaObjectType( const QString &sName, 
                                                  const QMetaObject *pMetaObject,
                                                  QScriptEngine::FunctionSignature  pFunction)
{
    Lock();
    QScriptValue ctor = m_engine.newFunction( pFunction );

    QScriptValue metaObject = m_engine.newQMetaObject( pMetaObject, ctor );
    m_engine.globalObject().setProperty( sName, metaObject );
    Unlock();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

ScriptInfo *ServerSideScripting::GetLoadedScript( const QString &sFileName )
{
    ScriptInfo *pInfo = nullptr;

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
                                        HTTPRequest *pRequest, const QByteArray &cspToken)
{
    try
    {

        ScriptInfo *pInfo = nullptr;

        // ------------------------------------------------------------------
        // See if page has already been loaded
        // ------------------------------------------------------------------

        pInfo = GetLoadedScript( sFileName );

        // ------------------------------------------------------------------
        // Load Script File and Create Function
        // ------------------------------------------------------------------

        QFileInfo  fileInfo ( sFileName );
        QDateTime  dtLastModified = fileInfo.lastModified();

        Lock();
        if ((pInfo == nullptr) || (pInfo->m_dtTimeStamp != dtLastModified ))
        {
            QString      sCode = CreateMethodFromFile( sFileName );

            QScriptValue func  = m_engine.evaluate( sCode, sFileName );

            if ( m_engine.hasUncaughtException() )
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Uncaught exception loading QSP File: %1 - (line %2) %3")
                        .arg(sFileName)
                        .arg(m_engine.uncaughtExceptionLineNumber())
                        .arg(m_engine.uncaughtException().toString()));

                Unlock();
                return false;
            }

            if (pInfo != nullptr)
            {
                pInfo->m_oFunc       = func;
                pInfo->m_dtTimeStamp = dtLastModified;
            }
            else
            {
                pInfo = new ScriptInfo( func, dtLastModified );
                m_mapScripts[ sFileName ] = pInfo;
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
        static const QRegularExpression validChars {
            R"(^([a-zA-Z]|_|\$)(\w|\$)+$)",
            QRegularExpression::UseUnicodePropertiesOption };

        QVariantMap params;
        QString prevArrayName = "";
        QVariantMap array;
        for (auto it = mapParams.cbegin(); it != mapParams.cend(); ++it)
        {
            const QString& key = it.key();
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

                auto match = validChars.match(arrayKey);
                if (!match.hasMatch()) // Discard anything that isn't valid for now
                    continue;

                array.insert(arrayKey, value);

                if ((it + 1) != mapParams.cend())
                    continue;
            }

            if (!array.isEmpty())
            {
                params.insert(prevArrayName, QVariant(array));
                array.clear();
            }
            // End Array handling

            auto match = validChars.match(key);
            if (!match.hasMatch()) // Discard anything that isn't valid for now
                continue;

            params.insert(key, value);

        }

        // ------------------------------------------------------------------
        // Build array of request headers
        // ------------------------------------------------------------------

        QStringMap mapHeaders = pRequest->m_mapHeaders;

        QVariantMap requestHeaders;
        for (auto it = mapHeaders.begin(); it != mapHeaders.end(); ++it)
        {
            QString key = it.key();
            key = key.replace('-', '_'); // May be other valid chars in a request header that we need to replace
            QVariant value = QVariant(it.value());

            auto match = validChars.match(key);
            if (!match.hasMatch()) // Discard anything that isn't valid for now
                continue;

            requestHeaders.insert(key, value);
        }

        // ------------------------------------------------------------------
        // Build array of cookies
        // ------------------------------------------------------------------

        QStringMap mapCookies = pRequest->m_mapCookies;

        QVariantMap requestCookies;
        for (auto it = mapCookies.begin(); it != mapCookies.end(); ++it)
        {
            QString key = it.key();
            key = key.replace('-', '_'); // May be other valid chars in a request header that we need to replace
            QVariant value = QVariant(it.value());

            auto match = validChars.match(key);
            if (!match.hasMatch()) // Discard anything that isn't valid for now
                continue;

            requestCookies.insert(key, value);
        }

        // ------------------------------------------------------------------
        // Build array of information from the server e.g. client IP
        // See RFC 3875 - The Common Gateway Interface
        // ------------------------------------------------------------------

        QVariantMap serverVars;
        //serverVars.insert("QUERY_STRING", QVariant())
        serverVars.insert("REQUEST_METHOD", QVariant(pRequest->GetRequestType()));
        serverVars.insert("SCRIPT_NAME", QVariant(sFileName));
        serverVars.insert("REMOTE_ADDR", QVariant(pRequest->GetPeerAddress()));
        serverVars.insert("SERVER_NAME", QVariant(pRequest->GetHostName()));
        serverVars.insert("SERVER_PORT", QVariant(pRequest->GetHostPort()));
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
        // User Session information
        //
        // SECURITY
        // The session token and password digest are considered sensitive on
        // unencrypted connections and therefore must never be included in the
        // HTML. An intercepted session token or password digest can be used
        // to login or hijack an existing session.
        // ------------------------------------------------------------------
        MythUserSession session = pRequest->m_userSession;
        QVariantMap sessionVars;
        sessionVars.insert("username", session.GetUserName());
        sessionVars.insert("userid", session.GetUserId());
        sessionVars.insert("created", session.GetSessionCreated());
        sessionVars.insert("lastactive", session.GetSessionLastActive());
        sessionVars.insert("expires", session.GetSessionExpires());

        // ------------------------------------------------------------------
        // Add the arrays (objects) we've just created to the global scope
        // They may be accessed as 'Server.REMOTE_ADDR'
        // ------------------------------------------------------------------

        m_engine.globalObject().setProperty("Parameters",
                                            m_engine.toScriptValue(params));
        m_engine.globalObject().setProperty("RequestHeaders",
                                            m_engine.toScriptValue(requestHeaders));
        QVariantMap respHeaderMap;
        m_engine.globalObject().setProperty("ResponseHeaders",
                                            m_engine.toScriptValue(respHeaderMap));
        m_engine.globalObject().setProperty("Server",
                                            m_engine.toScriptValue(serverVars));
        m_engine.globalObject().setProperty("Session",
                                            m_engine.toScriptValue(sessionVars));
        QScriptValue qsCspToken = m_engine.toScriptValue(cspToken);
        m_engine.globalObject().setProperty("CSP_NONCE", qsCspToken);


        // ------------------------------------------------------------------
        // Execute function to render output
        // ------------------------------------------------------------------

        OutputStream outStream( pOutStream );

        QScriptValueList args;
        args << m_engine.newQObject( &outStream );
        args << m_engine.toScriptValue(params);

        QScriptValue ret = pInfo->m_oFunc.call( QScriptValue(), args );

        if (ret.isError())
        {
            QScriptValue lineNo = ret.property( "lineNumber" );

            LOG(VB_GENERAL, LOG_ERR,
                QString("Error calling QSP File: %1(%2) - %3")
                    .arg(sFileName)
                    .arg( lineNo.toInt32 () )
                    .arg( ret   .toString() ));
            Unlock();
            return false;

        }

        if (m_engine.hasUncaughtException())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Uncaught exception calling QSP File: %1(%2) - %3")
                    .arg(sFileName)
                    .arg(m_engine.uncaughtExceptionLineNumber() )
                    .arg(m_engine.uncaughtException().toString()));
            Unlock();
            return false;
        }
        Unlock();
    }
    catch(...)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Exception while evaluating QSP File: %1") .arg(sFileName));

        Unlock();
        return false;
    }

    // Apply any custom headers defined by the script
    QVariantMap responseHeaders;
    responseHeaders = m_engine.fromScriptValue< QVariantMap >
                        (m_engine.globalObject().property("ResponseHeaders"));
    QVariantMap::iterator it;
    for (it = responseHeaders.begin(); it != responseHeaders.end(); ++it)
    {
        pRequest->SetResponseHeader(it.key(), it.value().toString(), true);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QString ServerSideScripting::CreateMethodFromFile( const QString &sFileName ) const
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
        sCode << "try {\n";

        while( !stream.atEnd() )
        {
            QString sLine = stream.readLine();

            bInCode = ProcessLine( sCode, sLine, bInCode, sTransBuffer );
        }
    
        sCode << "} catch( err ) { return err; }\n";
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
                                       QString     &sTransBuffer ) const
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

        sTransBuffer = " ";
        sTransBuffer.append(sLine.mid(nStartTransPos + 6).trimmed());
        sLine = sLine.left(nStartTransPos);
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
        {
            bMatchFound = true;
        }
        
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

                    QStringList sParts = sSegment.split( ' ', Qt::SkipEmptyParts );
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
                        {
                            LOG(VB_GENERAL, LOG_ERR,
                                QString("ServerSideScripting::ProcessLine 'import' - File not found: %1%2")
                                   .arg(m_sResRootPath, sFileName));
                        }
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR,
                            QString("ServerSideScripting::ProcessLine 'import' - Malformed [%1]")
                                .arg( sSegment ));
                    }

                }
                else
                {
                    sCode << sSegment << "\n";
                }

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

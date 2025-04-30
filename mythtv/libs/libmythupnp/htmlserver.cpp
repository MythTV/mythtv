//////////////////////////////////////////////////////////////////////////////
// Program Name: htmlserver.cpp
// Created     : Mar. 9, 2011
//
// Purpose     : Http server extension to serve up static html content
//                                                                            
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "htmlserver.h"

#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QUuid>

#include "libmythbase/mythlogging.h"
#include "libmythbase/storagegroup.h"

#include "httprequest.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HtmlServerExtension::HtmlServerExtension( const QString &sSharePath,
                                          const QString &sApplicationPrefix)
  : HttpServerExtension( "Html" , sSharePath),
    m_indexFilename(sApplicationPrefix + "index")
{
#if CONFIG_QTSCRIPT
    LOG(VB_HTTP, LOG_INFO, QString("HtmlServerExtension() - SharePath = %1")
            .arg(m_sSharePath));
    m_scripting.SetResourceRootPath( m_sSharePath );
#endif
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HtmlServerExtension::ProcessRequest( HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if ( !pRequest->m_sBaseUrl.startsWith("/"))
            return( false );

        if ((pRequest->m_eType != RequestTypeGet) &&
            (pRequest->m_eType != RequestTypeHead) &&
            (pRequest->m_eType != RequestTypePost))
        {
            pRequest->m_eResponseType = ResponseTypeHTML;
            pRequest->m_nResponseStatus = 405; // Method not allowed
            // Conservative list, we can't really know what methods we
            // actually allow for an arbitrary resource without some sort of
            // high maintenance database
            pRequest->m_response.write( pRequest->GetResponsePage() );
            pRequest->SetResponseHeader("Allow",  "GET, HEAD");
            return true;
        }

        bool      bStorageGroupFile = false;
        QFileInfo oInfo( m_sSharePath + pRequest->m_sResourceUrl );

        if (oInfo.isDir())
        {
            QString sIndexFileName = oInfo.filePath() + m_indexFilename + ".qsp";

            if (QFile::exists( sIndexFileName ))
                oInfo.setFile( sIndexFileName );
            else 
                oInfo.setFile( oInfo.filePath() + m_indexFilename + ".html" );
        }

        if (pRequest->m_sResourceUrl.startsWith("/StorageGroup/"))
        {
            StorageGroup oGroup(pRequest->m_sResourceUrl.section('/', 2, 2));
            QString      sFile =
                oGroup.FindFile(pRequest->m_sResourceUrl.section('/', 3));
            if (!sFile.isEmpty())
            {
                oInfo.setFile(sFile);
                bStorageGroupFile = true;
            }
        }

        if (bStorageGroupFile || oInfo.exists() )
        {
            QString sResName = oInfo.canonicalFilePath();

            // --------------------------------------------------------------
            // Checking for url's that contain ../ or similar.
            // --------------------------------------------------------------

            if (( bStorageGroupFile ) ||
                (sResName.startsWith( m_sSharePath, Qt::CaseInsensitive )))
            {
                if (oInfo.exists())
                {
                    if (oInfo.isSymLink())
                        sResName = oInfo.symLinkTarget();

                    // ------------------------------------------------------
                    // CSP Nonce
                    // ------------------------------------------------------
                    QByteArray cspNonce = QUuid::createUuid().toByteArray().toBase64();
                    cspNonce = cspNonce.mid(1, cspNonce.length() - 2); // UUID, with braces removed

                    // ------------------------------------------------------
                    // Is this a Qt Server Page (File contains script)...
                    // ------------------------------------------------------

                    QString sSuffix = oInfo.suffix().toLower();

                    QString sMimeType = HTTPRequest::GetMimeType(sSuffix);

                    if (sMimeType == "text/html")
                        pRequest->m_eResponseType = ResponseTypeHTML;
                    else if (sMimeType == "text/xml")
                        pRequest->m_eResponseType = ResponseTypeXML;
                    else if (sMimeType == "application/javascript")
                        pRequest->m_eResponseType = ResponseTypeJS;
                    else if (sMimeType == "text/css")
                        pRequest->m_eResponseType = ResponseTypeCSS;
                    else if (sMimeType == "text/plain")
                        pRequest->m_eResponseType = ResponseTypeText;
                    else if (sMimeType == "image/svg+xml" &&
                              sSuffix != "svgz") // svgz are pre-compressed
                        pRequest->m_eResponseType = ResponseTypeSVG;

                    // ---------------------------------------------------------
                    // Force IE into 'standards' mode
                    // ---------------------------------------------------------
                    pRequest->SetResponseHeader("X-UA-Compatible", "IE=Edge");

                    // ---------------------------------------------------------
                    // SECURITY: Set X-Content-Type-Options to 'nosniff'
                    //
                    // IE only for now. Prevents browsers ignoring the
                    // Content-Type header we supply and potentially executing
                    // malicious script embedded in an image or css file.
                    //
                    // Yes, really, you need to explicitly disable this sort of
                    // dangerous behaviour in 2015!
                    // ---------------------------------------------------------
                    pRequest->SetResponseHeader("X-Content-Type-Options",
                                                "nosniff");

                    // ---------------------------------------------------------
                    // SECURITY: Set Content Security Policy
                    //
                    // *No external content allowed*
                    //
                    // This is an important safeguard. Third party content
                    // should never be permitted. It compromises security,
                    // privacy and violates the key principal that the
                    // WebFrontend should work on an isolated network with no
                    // internet access. Keep all content hosted locally!
                    // ---------------------------------------------------------

                    // For now the following are disabled as we use xhr to
                    // trigger playback on frontends if we switch to triggering
                    // that through an internal request then these would be
                    // better enabled
                    //"default-src 'self'; "
                    //"connect-src 'self' https://services.mythtv.org; "

                    // FIXME: unsafe-inline should be phased out, replaced by nonce-{csp_nonce} but it requires
                    //        all inline event handlers and style attributes to be removed ...
                    QString cspPolicy = "script-src 'self' 'unsafe-inline' 'unsafe-eval' https://services.mythtv.org; " // QString('nonce-%1').arg(QString(cspNonce))
                                        "style-src 'self' 'unsafe-inline'; "
                                        "frame-src 'self'; "
                                        "object-src 'self'; " // TODO: When we no longer require flash for some browsers, change this to 'none'
                                        "media-src 'self'; "
                                        "font-src 'self'; "
                                        "img-src 'self'; "
                                        "form-action 'self'; "
                                        "frame-ancestors 'self'; ";

                    pRequest->SetResponseHeader("X-XSS-Protection", "1; mode=block");

                    // For standards compliant browsers
                    pRequest->SetResponseHeader("Content-Security-Policy",
                                                cspPolicy);
                    // For Internet Explorer
                    pRequest->SetResponseHeader("X-Content-Security-Policy",
                                                cspPolicy);

                    if ((sSuffix == "qsp") ||
                        (sSuffix == "qxml") ||
                        (sSuffix == "qjs" )) 
                    {
                        QTextStream stream( &pRequest->m_response );
                        
#if CONFIG_QTSCRIPT
                        m_scripting.EvaluatePage( &stream, sResName, pRequest, cspNonce);
#endif

                        return true;
                    }

                    // ------------------------------------------------------
                    // Return the file.
                    // ------------------------------------------------------

                    pRequest->FormatFileResponse( sResName );

                    return true;
                }
            }
        }

        // force return as a 404...
        pRequest->FormatFileResponse( "" );
    }

    return( true );
}


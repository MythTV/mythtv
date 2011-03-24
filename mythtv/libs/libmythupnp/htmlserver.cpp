//////////////////////////////////////////////////////////////////////////////
// Program Name: htmlserver.cpp
// Created     : Mar. 9, 2011
//
// Purpose     : Http server extension to serve up static html content
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

#include "mythverbose.h"
#include "htmlserver.h"

#include <QFileInfo>
#include <QDir>
#include <QTextStream>

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HtmlServerExtension::HtmlServerExtension( const QString sSharePath)
  : HttpServerExtension( "Html" , sSharePath)
{
    // Cache the absolute path for the share directory.

    QDir dir( sSharePath + "/html" );

    dir.makeAbsolute();

    m_sAbsoluteSharePath =  dir.absolutePath();

    if (getenv("MYTHHTMLDIR"))
    {
        QString sTempSharePath = getenv("MYTHHTMLDIR");
        if (!sTempSharePath.isEmpty())
            m_sAbsoluteSharePath = sTempSharePath;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HtmlServerExtension::~HtmlServerExtension( )
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HtmlServerExtension::ProcessRequest( HttpWorkerThread *, HTTPRequest *pRequest )
{
    if (pRequest)
    {
        if ( pRequest->m_sBaseUrl.startsWith("/") == false)
            return( false );

        // Temporary until we get authentication enabled
        if ((pRequest->m_sResourceUrl.startsWith("/setup")) &&
            (!getenv("MYTHHTMLSETUP")))
        {
            QTextStream os(&pRequest->m_response);
            os << "<html><body><h3>Web-based Setup is currently disabled.</h3>"
                  "</body></html>";

            pRequest->m_eResponseType = ResponseTypeHTML;
            pRequest->m_mapRespHeaders[ "Cache-Control" ] =
                "no-cache=\"Ext\", max-age = 0";

            VERBOSE(VB_IMPORTANT, QString("WARNING: Attempt to access "
                    "Web-based Setup which is currently disabled. URL: %1")
                    .arg(pRequest->m_sResourceUrl));
            return true;
        }

        QFileInfo oInfo( m_sAbsoluteSharePath + pRequest->m_sResourceUrl );

        if (oInfo.isDir())
        {
            QString sIndexFileName = oInfo.filePath() + "/index.qsp";

            if (QFile::exists( sIndexFileName ))
                oInfo.setFile( sIndexFileName );
            else 
                oInfo.setFile( oInfo.filePath() + "/index.html" );
        }

        if (oInfo.exists() == true )
        {
            oInfo.makeAbsolute();

            QString sResName = oInfo.canonicalFilePath();

            // --------------------------------------------------------------
            // Checking for url's that contain ../ or similar.
            // --------------------------------------------------------------

            if ( sResName.startsWith( m_sAbsoluteSharePath, Qt::CaseInsensitive ))
            {
                if (oInfo.exists())
                {
                    if (oInfo.isSymLink())
                        sResName = oInfo.symLinkTarget();

                    // ------------------------------------------------------
                    // Is this a Qt Server Page (File contains script)...
                    // ------------------------------------------------------

                    if (oInfo.suffix().compare( "qsp", Qt::CaseInsensitive ) == 0)
                    {

                        pRequest->m_eResponseType = ResponseTypeHTML;

                        QTextStream stream( &pRequest->m_response );
                        
                        m_Scripting.EvaluatePage( &stream, sResName );

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


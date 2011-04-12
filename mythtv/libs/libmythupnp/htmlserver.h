//////////////////////////////////////////////////////////////////////////////
// Program Name: htmlserver.h
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

#ifndef __HTMLSERVER_H__
#define __HTMLSERVER_H__

#include "httpserver.h"
#include "serverSideScripting.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// HtmlExtension Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HtmlServerExtension : public HttpServerExtension
{
    private:

        QString             m_sAbsoluteSharePath;
        ServerSideScripting m_Scripting;

    public:
                 HtmlServerExtension( const QString sSharePath);
        virtual ~HtmlServerExtension( );

        // Special case, this extension is called if no other extension
        // processes the request.  

        virtual QStringList GetBasePaths() { return QStringList(); }

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );

        QScriptEngine* ScriptEngine()
        {
            return &(m_Scripting.m_engine);
        }

};

#endif

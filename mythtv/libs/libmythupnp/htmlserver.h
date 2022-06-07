//////////////////////////////////////////////////////////////////////////////
// Program Name: htmlserver.h
// Created     : Mar. 9, 2011
//
// Purpose     : Http server extension to serve up static html content
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef HTMLSERVER_H
#define HTMLSERVER_H

#include "httpserver.h"
#if CONFIG_QTSCRIPT
#include "serverSideScripting.h"
#endif

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

#if CONFIG_QTSCRIPT
        ServerSideScripting m_scripting;
#endif
        QString             m_indexFilename;

    public:
                 HtmlServerExtension( const QString &sSharePath,
                                      const QString &sApplicationPrefix);
        ~HtmlServerExtension( ) override = default;

        // Special case, this extension is called if no other extension
        // processes the request.  

        QStringList GetBasePaths() override // HttpServerExtension
            { return {}; }

        bool ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension

#if CONFIG_QTSCRIPT
        QScriptEngine* ScriptEngine()
        {
            return &(m_scripting.m_engine);
        }
#endif
};

#endif // HTMLSERVER_H

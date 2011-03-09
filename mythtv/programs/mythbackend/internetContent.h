//////////////////////////////////////////////////////////////////////////////
// Program Name: internetContent.h
//
// Purpose - MythTV XML protocol HttpServerExtension
//
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#ifndef INTERNETCONTENT_H_
#define INTERNETCONTENT_H_

#include <QDomDocument>
#include <QMap>
#include <QDateTime>

#include "httpserver.h"

#include "mythcontext.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class InternetContent : public HttpServerExtension
{
    private:

        void    GetInternetSearch( HTTPRequest *pRequest );
        void    GetInternetSources( HTTPRequest *pRequest );
        void    GetInternetContent( HTTPRequest *pRequest );

    public:
                 InternetContent( const QString &sSharePath);
        virtual ~InternetContent();

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );

};

#endif

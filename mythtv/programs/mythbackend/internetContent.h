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

        static void    GetInternetSearch( HTTPRequest *pRequest );
        static void    GetInternetSources( HTTPRequest *pRequest );
        static void    GetInternetContent( HTTPRequest *pRequest );

    public:
                 explicit InternetContent( const QString &sSharePath);
        virtual ~InternetContent() = default;

        QStringList GetBasePaths() override; // HttpServerExtension

        bool     ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension

};

#endif

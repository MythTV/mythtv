#include <iostream>
using namespace std;

#include "httpcomms.h"

HttpComms::HttpComms(QUrl &url)
    : http(0)
{
    init(url);
}

#ifndef ANCIENT_QT
HttpComms::HttpComms(QUrl &url, QHttpRequestHeader &header)
{
    init(url, header);
}
#endif

HttpComms::~HttpComms()
{
    delete http;
}

void HttpComms::init(QUrl &url)
{
#ifndef ANCIENT_QT
    QHttpRequestHeader header("GET", url.encodedPathAndQuery());
    QString userAgent = "Mozilla/9.876 (X11; U; Linux 2.2.12-20 i686, en) "
                        "Gecko/25250101 Netscape/5.432b1";

    header.setValue("Host", url.host());
    header.setValue("User-Agent", userAgent);

    init(url, header);
#else
    cerr << "HttpComms does not work on QT 3.0 (No IMDB grabbing for you)\n";
#endif
}

#ifndef ANCIENT_QT
void HttpComms::init(QUrl &url, QHttpRequestHeader &header)
{
    http = new QHttp();
    Q_UINT16 port = 80;

    if (url.hasPort()) 
        port = url.port();
    
    http->setHost(url.host(), port);

    m_done = false;
    m_data = "";

    connect(http, SIGNAL(done(bool)), this, SLOT(done(bool)));
    connect(http, SIGNAL(stateChanged(int)), this, SLOT(stateChanged(int)));

    http->request(header);
}
#endif

void HttpComms::stop()
{
#ifndef ANCIENT_QT
    disconnect(http, 0, 0, 0);
    http->abort();
#endif
}

void HttpComms::done(bool error)
{
#ifndef ANCIENT_QT
    if (error)
    {
       cout << "MythVideo: NetworkOperation Error on Finish: "
            << http->errorString() << ".\n";
    }
    else
        m_data = QString(http->readAll());

    m_done = true;
#endif 
}

void HttpComms::stateChanged(int state)
{
    (void)state;
}


#include <iostream>
using namespace std;

#include "httpcomms.h"

HttpComms::HttpComms(QUrl &url)
{
    init(url);
}

HttpComms::HttpComms(QUrl &url, QHttpRequestHeader &header)
{
    init(url, header);
}

HttpComms::~HttpComms()
{
    delete http;
}

void HttpComms::init(QUrl &url)
{
    QHttpRequestHeader header("GET", url.encodedPathAndQuery());
    QString userAgent = "Mozilla/9.876 (X11; U; Linux 2.2.12-20 i686, en) "
                        "Gecko/25250101 Netscape/5.432b1";

    header.setValue("Host", url.host());
    header.setValue("User-Agent", userAgent);

    init(url, header);
}

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

void HttpComms::stop()
{
    disconnect(http, 0, 0, 0);
    http->abort();
}

void HttpComms::done(bool error)
{
    if (error)
    {
       cout << "MythVideo: NetworkOperation Error on Finish: "
            << http->errorString() << ".\n";
    }
    else
        m_data = QString(http->readAll());

    m_done = true; 
}

void HttpComms::stateChanged(int state)
{
    (void)state;
}


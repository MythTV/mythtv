#include <iostream>
#include <qapplication.h>
#include <unistd.h>
using namespace std;

#include "httpcomms.h"

HttpComms::HttpComms(QUrl &url)
         : http(0)
{
    init(url);
}

HttpComms::HttpComms(QUrl &url, int timeoutms)
         : http(0)
{
    init(url);
    m_timer = new QTimer();
    connect(m_timer, SIGNAL(timeout()), SLOT(timeout()));
    m_timer->start(timeoutms, TRUE);
}

HttpComms::HttpComms(QUrl &url, QHttpRequestHeader &header)
{
    init(url, header);
}

HttpComms::~HttpComms()
{
    if (m_timer)
        delete m_timer;

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

    m_debug = 0;
    m_redirectedURL = "";
    m_done = false;
    m_statusCode = 0;
    m_responseReason = "";
    m_timer = NULL;
    m_timeout = false;
    m_url = url.toString();

    connect(http, SIGNAL(done(bool)), this, SLOT(done(bool)));
    connect(http, SIGNAL(stateChanged(int)), this, SLOT(stateChanged(int)));
    connect(http, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)),
            this, SLOT(headerReceived(const QHttpResponseHeader &)));

    http->request(header);
}

void HttpComms::stop()
{
    disconnect(http, 0, 0, 0);
    http->abort();

    if (m_timer)
        m_timer->stop();
}

void HttpComms::done(bool error)
{
    if (error)
    {
       cerr << "HttpComms::done() - NetworkOperation Error on Finish: "
            << http->errorString() << " (" << error << ": url: " 
            << m_url.latin1() << endl;
    }
    else if (http->bytesAvailable())
    {
        m_data.resize(http->bytesAvailable());
        m_data = http->readAll();
    }
    if (m_debug > 1)
        cerr << "done: " << m_data.size() << " bytes.\n";

    m_done = true;

    if (m_timer)
        m_timer->stop();
}

void HttpComms::stateChanged(int state)
{
    if (m_debug > 1) 
    {
        switch (state) 
        {
            case QHttp::Unconnected: cerr << "unconnected\n"; break;
            case QHttp::HostLookup: break;
            case QHttp::Connecting: cerr << "connecting\n"; break;
            case QHttp::Sending: cerr << "sending\n"; break;
            case QHttp::Reading: cerr << "reading\n"; break;
            case QHttp::Connected: cerr << "connected\n"; break;
            case QHttp::Closing: cerr << "closing\n"; break;
            default: cerr << "unknown state: " << state << endl; break;
        }
    }
}

void HttpComms::headerReceived(const QHttpResponseHeader &resp)
{
    m_statusCode = resp.statusCode();
    m_responseReason = resp.reasonPhrase();

    if (m_debug > 1) 
    {
        cerr << "Got HTTP response: " << m_statusCode << ":" 
             << m_responseReason << endl;    
        cerr << "Keys: " << resp.keys().join(",") << endl;
    }

    if (resp.statusCode() >= 300 && resp.statusCode() <= 400) 
    {
        // redirection
        QString uri = resp.value("LOCATION");
        if (m_debug > 0)
            cerr << "Redirection to: " << uri << endl;
       
        m_redirectedURL = resp.value("LOCATION");
    }
}

void HttpComms::timeout() 
{
   cerr << "HttpComms::Timeout for url: " << m_url.latin1() << endl;
   m_timeout = true;
   m_done = true;
}


// getHttp - static function for grabbing http data for a url
//           this is a synchronous function, it will block according to the vars
QString HttpComms::getHttp(QString& url, int timeoutMS, int maxRetries, 
                           int maxRedirects)
{
    int redirectCount = 0;
    int timeoutCount = 0;
    QString res = "";
    HttpComms *httpGrabber = NULL; 
    int debug = 0;
    QString hostname = "";

    while (1) 
    {
        QUrl qurl(url);
        if (hostname == "")
            hostname = qurl.host();  // hold onto original host
        if (!qurl.hasHost())        // can occur on redirects to partial paths
            qurl.setHost(hostname);
        if (debug > 0)
            cerr << "getHttp: grabbing: " << qurl.toString() << endl;

        if (httpGrabber != NULL)
            delete httpGrabber; 
        httpGrabber = new HttpComms(qurl, timeoutMS);

        while (!httpGrabber->isDone())
        {
            qApp->processEvents();
            usleep(10000);
        }

        // Handle timeout
        if (httpGrabber->isTimedout())
        {
            if (debug > 0) 
                cerr << "timeout for url:" << url.latin1() << endl;
           
            // Increment the counter and check were not over the limit
            if (timeoutCount++ >= maxRetries)
            {
                cerr << "Failed to contact server for url: " << url.latin1()
                     << endl;
                break;
            }

            // Try again
            if (debug > 0) 
               cerr << "attempt # " << (timeoutCount+1) << "/" << maxRetries 
                    << " for url:" << url.latin1() << endl;

            continue;
        }

        // Check for redirection
        if (!httpGrabber->getRedirectedURL().isEmpty()) 
        {
            if (debug > 0) 
                cerr << "redirection:" 
                     << httpGrabber->getRedirectedURL().latin1() << " count:" 
                     << redirectCount << " max:" << maxRedirects << endl;
            if (redirectCount++ < maxRedirects)
                url = httpGrabber->getRedirectedURL();

            // Try again
            timeoutCount = 0;
            continue;
        }

        res = httpGrabber->getData();
        break;
    }

    delete httpGrabber;

    if (debug > 1)
        cerr << "Got " << res.length() << " bytes from url: '" 
             << url.latin1() << "'" << endl;
    if (debug > 2)
        cerr << res;

    return res;
}

// getHttpFile - static function for grabbing a file from an http url
//      this is a synchronous function, it will block according to the vars
bool HttpComms::getHttpFile(QString& filename, QString& url, int timeoutMS, 
                            int maxRetries, int maxRedirects)
{
    int redirectCount = 0;
    int timeoutCount = 0;
    QByteArray data(0);
    bool res = false;
    HttpComms *httpGrabber = NULL;
    int debug = 0;
    QString hostname = "";

    while (1)
    {
        QUrl qurl(url);
        if (hostname == "")
            hostname = qurl.host();  // hold onto original host
        if (!qurl.hasHost())        // can occur on redirects to partial paths
            qurl.setHost(hostname);
        if (debug > 0)
            cerr << "getHttp: grabbing: " << qurl.toString() << endl;

        if (httpGrabber != NULL)
            delete httpGrabber;
        httpGrabber = new HttpComms(qurl, timeoutMS);

        while (!httpGrabber->isDone())
        {
            qApp->processEvents();
            usleep(10000);
        }

        // Handle timeout
        if (httpGrabber->isTimedout())
        {
            if (debug > 0)
                cerr << "timeout for url:" << url.latin1() << endl;

            // Increment the counter and check were not over the limit
            if (timeoutCount++ >= maxRetries)
            {
                cerr << "Failed to contact server for url: " << url.latin1()
                     << endl;
                break;
            }

            // Try again
            if (debug > 0)
               cerr << "attempt # " << (timeoutCount+1) << "/" << maxRetries
                    << " for url:" << url.latin1() << endl;

            continue;
        }

        // Check for redirection
        if (!httpGrabber->getRedirectedURL().isEmpty())
        {
            if (debug > 0)
                cerr << "redirection:"
                     << httpGrabber->getRedirectedURL().latin1() << " count:"
                     << redirectCount << " max:" << maxRedirects << endl;
            if (redirectCount++ < maxRedirects)
                url = httpGrabber->getRedirectedURL();

            // Try again
            timeoutCount = 0;
            continue;
        }

        data = httpGrabber->getRawData();

        if (data.size() > 0)
        {
            if (debug > 0)
                cerr << "getHttpFile: saving to file: " << filename << endl;

            QFile file(filename);
            if (file.open( IO_WriteOnly ))
            {
                QDataStream stream(& file);
                stream.writeRawBytes( (const char*) (data), data.size() );
                file.close();
                res = true;
                if (debug > 0)
                    cerr << "getHttpFile: File saved OK" << endl;
            }
            else
                if (debug > 0)
                    cerr << "getHttpFile: Failed to open file for writing" << endl;
        }
        else
           if (debug > 0)
               cerr << "getHttpFile: nothing to save to file!" << endl;

        break;
    }

    if (debug > 1)
        cerr << "Got " << data.size() << " bytes from url: '"
             << url.latin1() << "'" << endl;

    delete httpGrabber;

    return res;
}

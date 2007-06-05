#include <mythtv/mythcontext.h>

#include "mythsoap.h"

void MythSoap::doSoapRequest(QString host, QString path, QString soapAction,
                             QString query)
{
    connect(&http, SIGNAL(done(bool)), this, SLOT(httpDone(bool)));

    QHttpRequestHeader header("POST", path);
    header.setValue("Host", host);
    header.setValue("SOAPAction",  soapAction);
    header.setContentType("text/xml");

    http.setHost(host);
    QByteArray bArray(query.utf8());
    bArray.resize(bArray.size() - 1);
    http.request(header, bArray);
}

QByteArray MythSoap::getResponseData()
{
    return m_data;
}

bool MythSoap::isDone()
{
    return m_done;
}

bool MythSoap::hasError()
{
    return m_error;
}

void MythSoap::httpDone(bool error)
{
    if (error)
    {
        cerr << "Error in mythsoap.o retrieving data: " << http.errorString() << endl << flush;
    }
    else
    {
        m_data = http.readAll();
        //cout << "Data: " << m_data.data() << endl << flush;
    }
    m_done = true;
}

MythSoap::MythSoap()
{
    m_done = false;
    m_error = false;
}

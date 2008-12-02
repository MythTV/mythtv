#include <iostream>
using namespace std;

#include "mythsoap.h"


MythSoap::MythSoap(QObject *parent) :
    QObject(parent), m_done(false), m_error(false)
{
}

void MythSoap::doSoapRequest(QString host, QString path, QString soapAction,
                             QString query)
{
    connect(&http, SIGNAL(done(bool)), this, SLOT(httpDone(bool)));

    QHttpRequestHeader header("POST", path);
    header.setValue("Host", host);
    header.setValue("SOAPAction",  soapAction);
    header.setContentType("text/xml");

    http.setHost(host);
    QByteArray bArray = query.toUtf8();
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
        cerr << "Error in mythsoap.o retrieving data: " <<
            http.errorString().toLocal8Bit().constData() << endl;
        m_error = true;
    }
    else
    {
        const qint64 len = http.bytesAvailable();
        // QMemArray::assign() will own this so we don't have to delete it
        char *buffer = new char[len + 1];
        if (buffer) 
        {
            http.read(buffer, len);
            buffer[len] = '\0';
            m_data = QByteArray(buffer, len + 1);
        }
        else
        {
            cerr << "Failed to alloc memory in mythsoap.o" << endl;
        }
    }
    m_done = true;
}

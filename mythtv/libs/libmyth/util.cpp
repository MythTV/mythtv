#include <qapplication.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "util.h"

bool WriteStringList(QSocket *socket, QStringList &list)
{
    QString str = list.join("[]:[]");
    QCString utf8 = str.utf8();

    int size = utf8.length();

    int written = 0;

    socket->writeBlock((const char *)&size, sizeof(int));

    while (size > 0)
    {
        int temp = socket->writeBlock(utf8.data() + written, size);
        written += temp;
        size -= temp;
        if (size > 0)
            qApp->processEvents();
    }

    qApp->lock();
    if (socket->bytesToWrite() > 0)
        socket->flush();
    qApp->unlock();

    return true;
}

bool ReadStringList(QSocket *socket, QStringList &list)
{
    list.clear();

    while (socket->waitForMore(5) < sizeof(int))
    {
        usleep(50);
    }

    int size;
    socket->readBlock((char *)&size, sizeof(int));

    QCString utf8(size + 1);

    int read = 0;
    while (size > 0)
    {
        int temp = socket->readBlock(utf8.data() + read, size);
        read += temp;
        size -= temp;
        if (size > 0)
            qApp->processEvents();
    }

    QString str = QString::fromUtf8(utf8.data());

    list = QStringList::split("[]:[]", str);

    return true;
}

void WriteBlock(QSocket *socket, void *data, int len)
{
    int size = len;
    int written = 0;

    while (size > 0)
    {
        int temp = socket->writeBlock((char *)data + written, size);
        written += temp;
        size -= temp;
        if (size > 0)
        {
            cerr << size << endl;
            qApp->processEvents();
        }
    }

    while (socket->bytesToWrite() > 0)
        socket->flush();
}

int ReadBlock(QSocket *socket, void *data, int maxlen)
{
    int read = 0;
    int size = maxlen;
    while (size > 0)
    {
        int temp = socket->readBlock((char *)data + read, size);
        read += temp;
        size -= temp;
        if (size > 0)
            qApp->processEvents();
    }

    return maxlen;
}

void encodeLongLong(QStringList &list, long long num)
{
    list << QString::number((int)(num >> 32));
    list << QString::number((int)(num & 0xffffffffLL));
}

long long decodeLongLong(QStringList &list, int offset)
{
    long long retval = 0;

    int l1 = list[offset].toInt();
    int l2 = list[offset + 1].toInt();
 
    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
} 

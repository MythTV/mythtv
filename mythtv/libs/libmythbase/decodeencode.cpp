// C++ headers
#include <iostream>

using namespace std;

// C headers
#include <cerrno>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

// System specific C headers
#include "compat.h"

// Qt headers
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QFont>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "compat.h"
#include "mythverbose.h"

#include "decodeencode.h"

/** \fn encodeLongLong(QStringList&,long long)
 *  \brief Encodes a long for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *   We encode the long long as strings representing two signed
 *   32 bit integers.
 *
 *  \sa decodeLongLong(QStringList&,uint)
 *      decodeLongLong(QStringList&,QStringList::const_iterator&)
 */
void encodeLongLong(QStringList &list, long long num)
{
    list << QString::number((int)(num >> 32));
    list << QString::number((int)(num & 0xffffffffLL));
}

/** \fn decodeLongLong(QStringList&,uint)
 *  \brief Inefficiently decodes a long encoded for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *
 *   The long long is represented as two signed 32 bit integers.
 *
 *   Note: This decode performs two O(n) linear searches of the list,
 *         The iterator decode function is much more efficient.
 *
 *  \param list   List to search for offset and offset+1 in.
 *  \param offset Offset in list where to find first 32 bits of
 *                long long.
 *  \sa encodeLongLong(QStringList&,long long),
 *      decodeLongLong(QStringList&,QStringList::const_iterator&)
 */
long long decodeLongLong(QStringList &list, uint offset)
{
    long long retval = 0;
    if (offset >= (uint)list.size())
    {
        VERBOSE(VB_IMPORTANT,
                "decodeLongLong() called with offset >= list size.");
        return retval;
    }

    int l1 = list[offset].toInt();
    int l2 = list[offset + 1].toInt();

    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
}

/** \fn decodeLongLong(QStringList&,QStringList::const_iterator&)
 *  \brief Decodes a long encoded for streaming in the MythTV protocol.
 *
 *   We need this for Qt3.1 compatibility, since it will not
 *   print or read a 64 bit number directly.
 *
 *   The long long is represented as two signed 32 bit integers.
 *
 *  \param list   List to search for offset and offset+1 in.
 *  \param it     Iterator pointing to first 32 bits of long long.
 *  \sa encodeLongLong(QStringList&,long long),
 *      decodeLongLong(QStringList&,uint)
 */
long long decodeLongLong(QStringList &list, QStringList::const_iterator &it)
{
    (void)list;

    long long retval = 0;

    bool ok = true;
    int l1=0, l2=0;

    if (it == list.end())
        ok = false;
    else
        l1 = (*(it++)).toInt();

    if (it == list.end())
        ok = false;
    else
        l2 = (*(it++)).toInt();

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT,
                "decodeLongLong() called with the iterator too close "
                "to the end of the list.");
        return 0;
    }

    retval = ((long long)(l2) & 0xffffffffLL) | ((long long)(l1) << 32);

    return retval;
}

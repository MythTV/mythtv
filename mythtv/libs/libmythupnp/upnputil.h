//////////////////////////////////////////////////////////////////////////////
// Program Name: upnputil.h
// Created     : Jan. 15, 2007
//
// Purpose     : Global Helper Methods...
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPUTIL_H__
#define __UPNPUTIL_H__

#include <QStringList>
#include <QMap>

#include "upnpexp.h"
#include "compat.h"     // for suseconds_t

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

template <class T> inline const T& Min( const T &x, const T &y ) 
{
    return( ( x < y ) ? x : y );
}
 
template <class T> inline const T& Max( const T &x, const T &y ) 
{
    return( ( x > y ) ? x : y );
}
//////////////////////////////////////////////////////////////////////////////
// Typedefs
//////////////////////////////////////////////////////////////////////////////

typedef struct timeval              TaskTime;
typedef QMap< QString, QString >    QStringMap;


/////////////////////////////////////////////////////////////////////////////

class NameValue;
class NameValues;
class NameValue
{
  public:
    QString sName;
    QString sValue;

    NameValues *pAttributes;

  public:
    NameValue() :
        sName(), sValue(), pAttributes(NULL) { }
    NameValue(const QString &name, const QString &value) :
        sName(name), sValue(value), pAttributes(NULL) { }
    NameValue(const QString &name, const char *value) :
        sName(name), sValue(value), pAttributes(NULL) { }
    NameValue(const QString &name, int value) :
        sName(name), sValue(QString::number(value)), pAttributes(NULL) { }
    NameValue(const QString &name, long value) :
        sName(name), sValue(QString::number(value)), pAttributes(NULL) { }
    NameValue(const QString &name, qlonglong value) :
        sName(name), sValue(QString::number(value)), pAttributes(NULL) { }
    NameValue(const QString &name, uint value) :
        sName(name), sValue(QString::number(value)), pAttributes(NULL) { }
    NameValue(const QString &name, ulong value) :
        sName(name), sValue(QString::number(value)), pAttributes(NULL) { }
    NameValue(const QString &name, qulonglong value) :
        sName(name), sValue(QString::number(value)), pAttributes(NULL) { }
    NameValue(const QString &name, bool value) :
        sName(name), sValue((value) ? "1" : "0"), pAttributes(NULL) { }
    inline NameValue(const NameValue &nv);
    inline NameValue& operator=(const NameValue &nv);

    inline ~NameValue();

    inline void AddAttribute(const QString &name, const QString &value);
};
class NameValues : public QList<NameValue> {};

inline NameValue::NameValue(const NameValue &nv) :
    sName(nv.sName), sValue(nv.sValue), pAttributes(NULL)
{
    if (nv.pAttributes)
    {
        pAttributes = new NameValues;
        *pAttributes = *nv.pAttributes;
    }
}

inline NameValue& NameValue::operator=(const NameValue &nv)
{
    if (this == &nv)
        return *this;

    sName  = nv.sName;
    sValue = nv.sValue;

    if (nv.pAttributes)
    {
        pAttributes = new NameValues;
        *pAttributes = *nv.pAttributes;
    }
    else
    {
        pAttributes = NULL;
    }

    return *this;
}

inline NameValue::~NameValue()
{
    if (pAttributes)
    {
        delete pAttributes;
        pAttributes = NULL;
    }
}

inline void NameValue::AddAttribute(const QString &name, const QString &value)
{
    if (!pAttributes)
        pAttributes = new NameValues();

    pAttributes->push_back(NameValue(name, value));
}

//////////////////////////////////////////////////////////////////////////////
// Global Function Prototypes
//////////////////////////////////////////////////////////////////////////////

QString LookupUDN         ( const QString     &sDeviceType );

bool operator<            ( TaskTime t1, TaskTime t2 );
bool operator==           ( TaskTime t1, TaskTime t2 );

void AddMicroSecToTaskTime( TaskTime &t, suseconds_t uSecs );
void AddSecondsToTaskTime ( TaskTime &t, long nSecs );

QByteArray gzipCompress( const QByteArray &data );

UPNP_PUBLIC QStringList GetSourceProtocolInfos ();
UPNP_PUBLIC QStringList GetSinkProtocolInfos ();

#endif

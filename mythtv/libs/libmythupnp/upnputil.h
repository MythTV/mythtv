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
    bool    bRequired;

    NameValues *pAttributes;

  public:
    NameValue() :
        sName(), sValue(), bRequired(false), pAttributes(NULL) { }
    NameValue(const QString &name, const QString &value, bool required = false) :
        sName(name), sValue(value), bRequired(required), pAttributes(NULL) { }
    NameValue(const QString &name, const char *value, bool required = false) :
        sName(name), sValue(value), bRequired(required), pAttributes(NULL) { }
    NameValue(const QString &name, int value, bool required = false) :
        sName(name), sValue(QString::number(value)), bRequired(required), pAttributes(NULL) { }
    NameValue(const QString &name, long value, bool required = false) :
        sName(name), sValue(QString::number(value)), bRequired(required), pAttributes(NULL) { }
    NameValue(const QString &name, qlonglong value, bool required = false) :
        sName(name), sValue(QString::number(value)), bRequired(required), pAttributes(NULL) { }
    NameValue(const QString &name, uint value, bool required = false) :
        sName(name), sValue(QString::number(value)), bRequired(required), pAttributes(NULL) { }
    NameValue(const QString &name, ulong value, bool required = false) :
        sName(name), sValue(QString::number(value)), bRequired(required), pAttributes(NULL) { }
    NameValue(const QString &name, qulonglong value, bool required = false) :
        sName(name), sValue(QString::number(value)), bRequired(required), pAttributes(NULL) { }
    NameValue(const QString &name, bool value, bool required = false) :
        sName(name), sValue((value) ? "1" : "0"), bRequired(required), pAttributes(NULL) { }
    inline NameValue(const NameValue &nv);
    inline NameValue& operator=(const NameValue &nv);

    inline ~NameValue();

    inline void AddAttribute(const QString &name, const QString &value, bool required);
    inline QString toXML();
};
class NameValues : public QList<NameValue> {};

inline NameValue::NameValue(const NameValue &nv) :
    sName(nv.sName), sValue(nv.sValue), bRequired(nv.bRequired), pAttributes(NULL)
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
    bRequired = nv.bRequired;

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

inline void NameValue::AddAttribute(const QString &name, const QString &value,
                                    bool required)
{
    if (!pAttributes)
        pAttributes = new NameValues();

    pAttributes->push_back(NameValue(name, value, required));
}


inline QString NameValue::toXML()
{
    QString sAttributes;
    QString attributeTemplate = " %1=\"%2\"";
    QString xml = "<%1%2>%3</%1>";

    NameValues::const_iterator it;
    for (it = pAttributes->constBegin(); it != pAttributes->constEnd(); ++it)
    {
        sAttributes += attributeTemplate.arg((*it).sName).arg((*it).sValue);
    }

    return xml.arg(sName).arg(sAttributes).arg(sValue);
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

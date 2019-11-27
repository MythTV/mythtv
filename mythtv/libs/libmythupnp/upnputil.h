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

using TaskTime   = struct timeval;
using QStringMap = QMap< QString, QString >;


/////////////////////////////////////////////////////////////////////////////

class NameValue;
class NameValues;
class NameValue
{
  public:
    QString     m_sName;
    QString     m_sValue;
    bool        m_bRequired   {false};

    NameValues *m_pAttributes {nullptr};

  public:
    NameValue() = default;
    NameValue(const QString &name, const QString &value, bool required = false) :
        m_sName(name), m_sValue(value), m_bRequired(required) { }
    NameValue(const QString &name, const char *value, bool required = false) :
        m_sName(name), m_sValue(value), m_bRequired(required) { }
    NameValue(const QString &name, int value, bool required = false) :
        m_sName(name), m_sValue(QString::number(value)), m_bRequired(required) { }
    NameValue(const QString &name, long value, bool required = false) :
        m_sName(name), m_sValue(QString::number(value)), m_bRequired(required) { }
    NameValue(const QString &name, qlonglong value, bool required = false) :
        m_sName(name), m_sValue(QString::number(value)), m_bRequired(required) { }
    NameValue(const QString &name, uint value, bool required = false) :
        m_sName(name), m_sValue(QString::number(value)), m_bRequired(required) { }
    NameValue(const QString &name, ulong value, bool required = false) :
        m_sName(name), m_sValue(QString::number(value)), m_bRequired(required) { }
    NameValue(const QString &name, qulonglong value, bool required = false) :
        m_sName(name), m_sValue(QString::number(value)), m_bRequired(required) { }
    NameValue(const QString &name, bool value, bool required = false) :
        m_sName(name), m_sValue((value) ? "1" : "0"), m_bRequired(required) { }
    inline NameValue(const NameValue &nv);
    inline NameValue& operator=(const NameValue &nv);

    inline ~NameValue();

    inline void AddAttribute(const QString &name, const QString &value, bool required);
    inline QString toXML();
};
class NameValues : public QList<NameValue> {};

inline NameValue::NameValue(const NameValue &nv) :
    m_sName(nv.m_sName), m_sValue(nv.m_sValue), m_bRequired(nv.m_bRequired)
{
    if (nv.m_pAttributes)
    {
        m_pAttributes = new NameValues;
        *m_pAttributes = *nv.m_pAttributes;
    }
}

inline NameValue& NameValue::operator=(const NameValue &nv)
{
    if (this == &nv)
        return *this;

    m_sName  = nv.m_sName;
    m_sValue = nv.m_sValue;
    m_bRequired = nv.m_bRequired;

    if (nv.m_pAttributes)
    {
        m_pAttributes = new NameValues;
        *m_pAttributes = *nv.m_pAttributes;
    }
    else
    {
        m_pAttributes = nullptr;
    }

    return *this;
}

inline NameValue::~NameValue()
{
    delete m_pAttributes;
    m_pAttributes = nullptr;
}

inline void NameValue::AddAttribute(const QString &name, const QString &value,
                                    bool required)
{
    if (!m_pAttributes)
        m_pAttributes = new NameValues();

    m_pAttributes->push_back(NameValue(name, value, required));
}


inline QString NameValue::toXML()
{
    QString sAttributes;
    QString attributeTemplate = " %1=\"%2\"";
    QString xml = "<%1%2>%3</%1>";

    NameValues::const_iterator it;
    for (it = m_pAttributes->constBegin(); it != m_pAttributes->constEnd(); ++it)
    {
        sAttributes += attributeTemplate.arg((*it).m_sName).arg((*it).m_sValue);
    }

    return xml.arg(m_sName).arg(sAttributes).arg(m_sValue);
}

//////////////////////////////////////////////////////////////////////////////
// Global Function Prototypes
//////////////////////////////////////////////////////////////////////////////

QString LookupUDN         ( const QString     &sDeviceType );

bool operator<            ( TaskTime t1, TaskTime t2 );
bool operator==           ( TaskTime t1, TaskTime t2 );

void AddMicroSecToTaskTime( TaskTime &t, suseconds_t uSecs );
void AddSecondsToTaskTime ( TaskTime &t, long nSecs );

UPNP_PUBLIC QStringList GetSourceProtocolInfos ();
UPNP_PUBLIC QStringList GetSinkProtocolInfos ();

#endif

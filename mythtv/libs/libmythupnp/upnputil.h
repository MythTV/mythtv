//////////////////////////////////////////////////////////////////////////////
// Program Name: upnputil.h
//                                                                            
// Purpose - 
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPUTIL_H__
#define __UPNPUTIL_H__

#include <qptrlist.h>
#include <qstringlist.h>
#include <qmap.h>

// __suseconds_t doesn't exist on some older Unixes. e.g. Darwin/Mac OS X

#if defined(__FreeBSD__) 
#define __suseconds_t_defined  // It exists on FreeBSD, but doesn't define this
#endif

#ifndef __suseconds_t_defined
#define __suseconds_t_defined
typedef int32_t __suseconds_t;
#endif

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

class NameValueList : public QPtrList< NameValue > 
{
    public:

        NameValueList()
        {
            setAutoDelete( true );
        }       
};

class NameValue
{
    public:

        QString sName;
        QString sValue;

        NameValueList *pAttributes;

    NameValue() 
      : sName( NULL ), sValue( NULL ), pAttributes( NULL ) { }

    NameValue( const QString &name, const QString &value ) 
      : sName( name ), sValue( value ), pAttributes( NULL ) { }

    NameValue( const QString &name, char *value ) 
      : sName( name ), sValue( value ), pAttributes( NULL ) { }

    NameValue( const QString &name, int value ) 
      : sName( name ), sValue( QString::number( value )), pAttributes( NULL ) { }

    NameValue( const QString &name, bool value ) 
      : sName( name ), sValue( (value) ? "1" : "0" ), pAttributes( NULL ) { }

    ~NameValue()
    {
        if (pAttributes != NULL)
            delete pAttributes;
    }

    void AddAttribute( const QString &name, const QString &value )
    {
        if (pAttributes == NULL)
            pAttributes = new NameValueList();

        pAttributes->append( new NameValue( name, value ));
    }

};

//////////////////////////////////////////////////////////////////////////////
// Global Function Prototypes
//////////////////////////////////////////////////////////////////////////////

QString LookupUDN         ( QString      sDeviceType );
long    GetIPAddressList  ( QStringList &sStrList    );

bool operator<            ( TaskTime t1, TaskTime t2 );
bool operator==           ( TaskTime t1, TaskTime t2 );

void AddMicroSecToTaskTime( TaskTime &t, __suseconds_t uSecs );
void AddSecondsToTaskTime ( TaskTime &t, long nSecs );

#endif

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

#include "mythcontext.h"

// __suseconds_t doesn't exist on some older Unixes. e.g. Darwin/Mac OS X

#if defined(__FreeBSD__) 
#define __suseconds_t_defined  // It exists on FreeBSD, but doesn't define this
#endif

#ifndef __suseconds_t_defined
#define __suseconds_t_defined
typedef int32_t __suseconds_t;
#endif

//////////////////////////////////////////////////////////////////////////////
// Typedefs
//////////////////////////////////////////////////////////////////////////////

typedef struct timeval                   TaskTime;

/////////////////////////////////////////////////////////////////////////////

typedef struct _NameValue
{   
    QString sName;
    QString sValue;

    _NameValue( const QString &name, const QString value ) 
        : sName( name ), sValue( value ) { }

} NameValue;

class NameValueList : public QPtrList< NameValue > 
{
    public:

        NameValueList()
        {
            setAutoDelete( true );
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

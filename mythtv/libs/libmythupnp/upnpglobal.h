//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpglobal.h
//                                                                            
// Purpose - 
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPGLOBAL_H__
#define __UPNPGLOBAL_H__

#include "mythcontext.h"

// __suseconds_t doesn't exist on some older Unixes. e.g. Darwin/Mac OS X

#ifndef __suseconds_t_defined
#define __suseconds_t_defined
typedef int32_t __suseconds_t;
#endif

//////////////////////////////////////////////////////////////////////////////
// Typedefs
//////////////////////////////////////////////////////////////////////////////

typedef struct timeval                   TaskTime;

//////////////////////////////////////////////////////////////////////////////
// Global Function Prototypes
//////////////////////////////////////////////////////////////////////////////

QString LookupUDN         ( QString      sDeviceType );
long    GetIPAddressList  ( QStringList &sStrList    );

bool operator<            ( TaskTime t1, TaskTime t2 );
bool operator==           ( TaskTime t1, TaskTime t2 );

void AddMicroSecToTaskTime( TaskTime &t, __suseconds_t uSecs );

#endif

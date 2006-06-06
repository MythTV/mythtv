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

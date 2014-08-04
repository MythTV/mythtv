//////////////////////////////////////////////////////////////////////////////
// Program Name: rttiServices.h
// Created     : July 25, 2014
//
// Purpose - Myth Services API Interface definition 
//
// Copyright (c) 2014 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef RTTISERVICES_H_
#define RTTISERVICES_H_

#include "service.h"
#include "datacontracts/enum.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Notes -
//
//  * This implementation can't handle declared default parameters
//
//  * When called, any missing params are sent default values for its datatype
//
//  * Q_CLASSINFO( "<methodName>_Method", ...) is used to determine HTTP method
//    type.  Defaults to "BOTH", available values:
//          "GET", "POST" or "BOTH"
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC RttiServices : public Service  //, public QScriptable ???
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "4.0" );

    public:

        // Must call InitializeCustomTypes for each unique Custom Type used
        // in public slots below.

        RttiServices( QObject *parent = 0 ) : Service( parent )
        {
            DTC::Enum     ::InitializeCustomTypes();
        }

    public slots:

        virtual DTC::Enum* GetEnum   ( const QString   &FQN ) = 0;
};

#endif


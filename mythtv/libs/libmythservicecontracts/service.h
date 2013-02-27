//////////////////////////////////////////////////////////////////////////////
// Program Name: service.h
// Created     : Jan. 19, 2010
//
// Purpose     : Base class for all Web Services 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SERVICE_H_
#define SERVICE_H_

#include <QObject>
#include <QMetaType>
#include <QVariant>
#include <QFileInfo>
#include <QDateTime>
#include <QString>

#include "serviceexp.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Notes for derived classes -
//
//  * This implementation can't handle declared default parameters
//
//  * When called, any missing params are sent default values for its datatype
//
//  * Q_CLASSINFO( "<methodName>_Method", "BOTH" ) 
//    is used to determine HTTP method type.  
//    Defaults to "BOTH", available values:
//          "GET", "POST" or "BOTH"
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Service : public QObject 
{
    Q_OBJECT

    public:

        inline Service( QObject *parent = NULL );

    public:

        /////////////////////////////////////////////////////////////////////
        // This method should be overridden to handle non-QObject based custom types
        /////////////////////////////////////////////////////////////////////

        virtual QVariant ConvertToVariant  ( int nType, void *pValue );

        virtual void* ConvertToParameterPtr( int            nTypeId,
                                             const QString &sParamType, 
                                             void*          pParam,
                                             const QString &sValue );

        static bool ToBool( const QString &sVal );

};

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_DECLARE_METATYPE( QFileInfo )
inline Service::Service(QObject *parent) : QObject(parent)
{
    qRegisterMetaType< QFileInfo >();
}
#else
inline Service::Service(QObject *parent) : QObject(parent) {}
#endif

#endif

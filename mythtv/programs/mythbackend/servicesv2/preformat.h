//////////////////////////////////////////////////////////////////////////////
// Program Name: preformat.h
//
// Purpose - General return class for services that preformat the
//   return data (e.g. legacy status service)
//
// Created By  : Peter Bennett
//
//////////////////////////////////////////////////////////////////////////////

#ifndef PREFORMAT_H_
#define PREFORMAT_H_

#include <QBuffer>

#include "libmythbase/http/mythhttpservice.h"

// Legacy return type for the status service
class Preformat  : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "preformat", "true" );

    SERVICE_PROPERTY2   ( QString   , mimetype)
    SERVICE_PROPERTY2   ( QString   , buffer )

    public:
        Q_INVOKABLE Preformat(QObject *parent = nullptr)
        : QObject ( parent )
        {
        }

    private:
        Q_DISABLE_COPY(Preformat);
};

Q_DECLARE_METATYPE(Preformat*)

#endif
